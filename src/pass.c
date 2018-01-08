#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wordexp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h> /* for close() */

#ifdef __linux__
#include <sys/sendfile.h>
#endif

#ifdef __APPLE__
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/types.h>
#endif

#define RIO_BUFSIZE 1024
#define BUFFER_SIZE 1024
#define MAXLINE 1024
#define MAX_FILENAME 512

#define REQUIRE_RANGE_TRUE 1
#define  REQUIRE_RANGE_FALSE 0

/* describing a robust-i/o buffer */
typedef struct
{
  int RIOfd;                 /* descriptor for this buf */
  off_t RIOrest;                /* unread byte in this buf */
  char *RIObufferPTR;           /* next unread byte in this buf */
  char RIObuffer[RIO_BUFSIZE];  /* internal buffer */
} riobuffer_t;

/* describing a request for a file */
typedef struct {
  off_t offset;              /* for support Range */
  off_t end;
  int requireRange;
} httpRquest;

/* initialize a robst-i/o buffer */
void RIOreadInitBuffer(riobuffer_t *rp, int fd)
{
  rp->RIOfd = fd;
  rp->RIOrest = 0;
  rp->RIObufferPTR = rp->RIObuffer;
}

/* identical to read() */
ssize_t RIOread(riobuffer_t *rp, char *usrbuf, ssize_t n)
{
  int rest;

  while(rp->RIOrest <= 0) /* refill if buffer is empty */
  {
    rp->RIOrest = read(rp->RIOfd, rp->RIObuffer, sizeof(rp->RIObuffer));
    if (rp->RIOrest < 0) /* Interrupted by sig handler return */
    {
      if (errno != EINTR) return -1;
      else if (rp->RIOrest == 0) return 0; /* EOF */
      else rp->RIObufferPTR = rp->RIObuffer; /* reset buffer ptr */
    }
  }

  /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
  rest = n;
  if (rp->RIOrest < n) rest = rp->RIOrest;
  memcpy(usrbuf, rp->RIObufferPTR, rest);
  rp->RIObufferPTR += rest;
  rp->RIOrest -= rest;
  return rest;
}

/* read a line into buffer */
ssize_t RIOreadlineB(riobuffer_t *rp, void *usrbuf, size_t maxlen)
{
  int n;
  char c, *bufferPTR = usrbuf;

  for (n = 1; (size_t) n < maxlen; n++)
  {
    int readCount;
    if ((readCount = RIOread(rp, &c, 1)) == 1)
    {
      *bufferPTR++ = c;
      if (c == '\n') break;
    }
    else if (readCount == 0)
    {
      if (n == 1) return 0; /* EOF, no data read */
      else break;    /* EOF, some data was read */
    }
    else return -1;    /* error */
  }
  *bufferPTR = 0;
  return n;
}

/* numbered wirte() */
ssize_t RIOwriteN(int fd, void *usrbuf, size_t n)
{
  size_t nleft = n;
  char *bufferPTR = usrbuf;

  while (nleft > 0)
  {
    ssize_t numberWritten;
    if ((numberWritten = write(fd, bufferPTR, nleft)) <= 0)
    {
      if (errno == EINTR)  /* interrupted by sig handler return */
        numberWritten = 0;    /* and call write() again */
      else return -1;       /* errorno set by write() */
    }
    nleft -= numberWritten;
    bufferPTR += numberWritten;
  }
  return n;
}


/* find file name from path */
void findFilename(char *filepath, char* filename)
{
  size_t pt;
  size_t filenamePt = 0;

  for (pt = 0; pt <= strlen(filepath); pt++)
  {
    char ch;
    ch = filepath[pt];
    /* puts(&ch); */
    filename[filenamePt] = ch;
    filenamePt++;
    if(ch == '/')
    {
      bzero(filename, sizeof(&filename));
      filenamePt = 0;
    }
  }

  filename[filenamePt + 1] = '\0';
}

/* print error message and exit */
void errorExit(char* text)
{
  perror(text);
  exit(-1);
}


void readHeaderFromClient(int socketFD, httpRquest *request)
{
  char buffer[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];

  /* read the first line */
  riobuffer_t rioBuffer;
  RIOreadInitBuffer(&rioBuffer, socketFD);
  RIOreadlineB(&rioBuffer, buffer, MAXLINE);
  sscanf(buffer, "%s %s %s", method, url, version);

  /* if not GET close connection */
  if (strcmp(method, "GET"))
  {
    printf("not GET request:\n%s\n", buffer);
    close(socketFD);
    exit(0);
  }

  /* read lines and look for Range */
  request->requireRange = REQUIRE_RANGE_FALSE;
  request->offset = 0;
  request->end = 0;
  while (buffer[0] != '\n' && buffer[1] != '\n') /* end of header */
  {
    /* printf("%s", buffer); */
    RIOreadlineB(&rioBuffer, buffer, MAXLINE);
    if (buffer[0] == 'R' && buffer[1] == 'a' && buffer[2] == 'n') /* find "Range" field */
    {
      /* have range! */
      request->requireRange = REQUIRE_RANGE_TRUE;
      /* if cannot retrieve information from line */
      if (sscanf(buffer, "Range: bytes=%lld-%lld", &request->offset, &request->end) <= 0)
        {
          fprintf(stderr, "failed to read Range: %s\n", buffer);
          request->requireRange = REQUIRE_RANGE_FALSE;
          continue;
        }
      /* if bad range request */
      if (request->offset < 0 || request->end < 0 || request->offset > request->end ||
          (request->offset == 0 && request->end == 0))
        /* "Range: bytes=0-" will make both number 0 */
        {
          fprintf(stderr, "bad Range request or not supported: %s\n", buffer);
          request->requireRange = REQUIRE_RANGE_FALSE;
        }
    }
  }
}


off_t getFileLength(char *filepath)
{
  off_t fileLength;
  struct stat statBuffer;

  /* if not successfully read stat of file or file is not a regular file */
  if (stat(filepath, &statBuffer) != 0 || (!S_ISREG(statBuffer.st_mode)))
    errorExit("checking status of file failed, is it a regular file?");

  fileLength = statBuffer.st_size;
  return fileLength;
}

/* compose the header to be sent back to client */
void composeHeader(char *header, httpRquest *request, char *filepath)
{
  /* file name have to be shorter than MAX_FILENAME(1024) characters*/
  char filename[MAX_FILENAME];
  findFilename(filepath, filename);

  /* get file length */
  off_t fileLength;
  fileLength = getFileLength(filepath);
  if (fileLength < 0) errorExit("getting file length failed");

  /* if client want the whole file */
  if (request->requireRange == REQUIRE_RANGE_FALSE)
  {
    sprintf(header,
            "HTTP/1.1 200 OK\r\n"
            "Accept-Ranges: bytes\r\n"
            "Content-Disposition: attachment; filename=\"%s\"\r\n"
            "Content-Length: %lld\r\n"
            "\r\n", filename, fileLength);
  }
  /* if client want some part of the file */
  else if (request->requireRange == REQUIRE_RANGE_TRUE)
  {
    sprintf(header,
            "HTTP/1.1 206 Partial\r\n"
            "Accept-Ranges: bytes\r\n"
            "Content-Disposition: attachment; filename=\"%s\"\r\n"
            "Content-Range: bytes %lld-%lld/%lld\r\n"
            "Content-Length: %lld\r\n"
            "Content-Type: multipart/byteranges\r\n"
            "\r\n", filename, request->offset, request->end, fileLength, request->end - request->offset + 1);
    /* printf("offset: %lld\nend: %lld\n", request->offset, request->end); */
  }
}

#ifdef __APPLE__
/* send file and garantee every bytes are sent */
void sendFile(char *filepath, int clientSocketFD, httpRquest *request)
{
  /* open file */
  FILE *file = fopen(filepath, "rb");
  if (!file) errorExit("failed to open file");

  /* if client want whole file, end will be 0 */
  /* then set end to the last byte of file */
  if (request->requireRange == REQUIRE_RANGE_FALSE)
    request->end = getFileLength(filepath);

  off_t totalSize = request->end - request->offset + 1;
  off_t offset = request->offset; /* where to start to send in every cycle*/
  off_t totalBytesSent = 0;
  off_t len = BUFFER_SIZE; /* chunk size */

  /* send file in chunks */
  /* printf("totalBytesSent: %lld\n", totalBytesSent); */
  while (totalBytesSent < totalSize)
  {
    int ret; /* return code */
    if ((totalSize - totalBytesSent) < BUFFER_SIZE)
      len = totalSize - totalBytesSent;

    ret = sendfile(fileno(file), clientSocketFD, offset, &len, NULL, 0);
    if (ret < 0) errorExit("failed to send file");
    /* len is changed to how many bytes are sent */
    totalBytesSent += len;
    offset += len;
  }
}
#endif

#ifdef __linux__
/* send file and garantee every bytes are sent */
void sendFile(char *filepath, int clientSocketFD, httpRquest *request)
{
  /* open file */
  FILE *file = fopen(filepath, "rb");
  if (!file) errorExit("failed to open file");

  /* if client want whole file, end will be 0 */
  /* then set end to the last byte of file */
  if (request->requireRange == REQUIRE_RANGE_FALSE)
    request->end = getFileLength(filepath);

  off_t offset = request->offset;
  off_t bytesLeftToSend;
  off_t bytesSentInOneAction = 0;
  size_t len = BUFFER_SIZE;
  int fileNo = fileno(file);

  /* do not use request->offset from this point!!! */
  /* instead use offset */

  bytesLeftToSend = request->end - offset + 1;
  while (bytesLeftToSend > 0)
  {
    if (bytesLeftToSend < BUFFER_SIZE) len = bytesLeftToSend;

    bytesSentInOneAction = sendfile(clientSocketFD, fileNo, &offset, len);

    if (bytesSentInOneAction < 0) errorExit("failed to send file");

    offset += bytesSentInOneAction;
    bytesLeftToSend -= bytesSentInOneAction;
  }
}
#endif

/* read header from client and send response header and file */
void serveFile(int clientSocketFD, char *filepath)
{
  httpRquest request;
  readHeaderFromClient(clientSocketFD, &request);

  char header[BUFFER_SIZE];
  composeHeader(header, &request, filepath);
  RIOwriteN(clientSocketFD, header, strlen(header));
  sendFile(filepath, clientSocketFD, &request);
}

/* set up the connection and start listening on socket */
void initListening(int socketFD, struct sockaddr_in6 *address, int port)
{
  /* configure socket */
  address->sin6_family = AF_INET6;
  address->sin6_port = htons(port); /* host to network short */
  address->sin6_addr = in6addr_any;

  /* bind & listen */
  if(bind(socketFD, (struct sockaddr*) address, sizeof(*address)) != 0)
    errorExit("binding failed");

  if (listen(socketFD, 16)!=0) errorExit("listening failed");
}

/* serve a file over port */
void serve(char* filepath, int port)
{
  int socketFD;
  struct sockaddr_in6 address;

  if ((socketFD = socket(PF_INET6, SOCK_STREAM, 0)) < 0)
    errorExit("open socket failed");

  initListening(socketFD, &address, port);

  /* accept client */
  for (;;)
  {
    socklen_t size = sizeof(address);
    int clientSocketFD = accept(socketFD, (struct sockaddr*) &address, &size);
    if ( clientSocketFD < 0)
    {
      perror("failed to accept connection\n");
      continue;
    }

    if (fork() == 0)
    {
      /* puts("fork"); */
      shutdown(socketFD, SHUT_RDWR);
      close(socketFD);

      serveFile(clientSocketFD, filepath);
      shutdown(clientSocketFD, SHUT_RDWR);
      sleep(3);
      close(clientSocketFD);
      /* puts("end"); */
      exit(0);
    }
    else
    {
      close(clientSocketFD);
    }
  }
}

/* int main(int argc, char *argv[]) */
/* { */
/*   if (strcmp(argv[1], "-h") == 0) */
/*   { */
/*     puts("format: pass <filepath> <port>"); */
/*     puts("file name(not path) must not exceed 1024 characters"); */
/*     puts("keyboard interrupt to kill"); */
/*     return 0; */
/*   } */

/*   if (argc < 3 || argc > 3) */
/*   { */
/*     fprintf(stderr, "needs 2 arguments: file path & port\n"); */
/*     return -1; */
/*   } */

/*   char *filepath = argv[1]; */
/*   int port = atoi(argv[2]); */

/*   printf("serving  %s  on port  %d\n" */
/*          "keyboard interrupt to kill\n", filepath, port); */

/*   expandFilePath(filepath); */
/*   serve(filepath, port); */
/* } */
