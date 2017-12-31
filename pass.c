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

typedef struct
{
  int RIOfd;                 /* descriptor for this buf */
  off_t RIOrest;                /* unread byte in this buf */
  char *RIObufferPTR;           /* next unread byte in this buf */
  char RIObuffer[RIO_BUFSIZE];  /* internal buffer */
} riobuffer_t;

typedef struct {
  off_t offset;              /* for support Range */
  off_t end;
  int requireRange;
} httpRquest;

void RIOreadInitBuffer(riobuffer_t *rp, int fd)
{
  rp->RIOfd = fd;
  rp->RIOrest = 0;
  rp->RIObufferPTR = rp->RIObuffer;
}

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

ssize_t RIOreadlineB(riobuffer_t *rp, void *usrbuf, size_t maxlen)
{
  int n, readCount;
  char c, *bufferPTR = usrbuf;

  for (n = 1; (size_t) n < maxlen; n++)
  {
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

ssize_t RIOwriteN(int fd, void *usrbuf, size_t n)
{
  size_t nleft = n;
  ssize_t numberWritten;
  char *bufferPTR = usrbuf;

  while (nleft > 0)
  {
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
  char ch;

  for (pt = 0; pt < strlen(filepath); pt++)
  {
    ch = filepath[pt];
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

void errorExit(char* text)
{
  perror(text);
  exit(0);
}

void expandFilePath(char* filepath)
{
  wordexp_t wordExpand;

  if (wordexp(filepath, &wordExpand, 0) != 0)
    errorExit("expanding file path failed");
  filepath = wordExpand.we_wordv[0];
}


void readHeaderFromClient(int socketFD, httpRquest *request)
{
  char buffer[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];

  riobuffer_t rioBuffer;
  RIOreadInitBuffer(&rioBuffer, socketFD);
  sscanf(buffer, "%s %s %s", method, url, version);

  request->requireRange = REQUIRE_RANGE_FALSE;
  request->offset = 0;
  request->end = 0;
  while (buffer[0] != '\n' && buffer[1] != '\n')
  {
    RIOreadlineB(&rioBuffer, buffer, MAXLINE);
    if (buffer[0] == 'R' && buffer[1] == 'a' && buffer[2] == 'n') /* find "Range" field */
    {
      request->requireRange = REQUIRE_RANGE_TRUE;
      sscanf(buffer, "Range: bytes=%lld-%lld", &request->offset, &request->end);
      request->end++;
    }
  }
}


off_t getFileLength(char *filepath)
{
  off_t fileLength;
  struct stat statBuffer;

  if (stat(filepath, &statBuffer) != 0 || (!S_ISREG(statBuffer.st_mode)))
    errorExit("checking status of file failed, is it a regular file?");

  fileLength = statBuffer.st_size;
  return fileLength;
}

void composeHeader(char *header, httpRquest *request, char *filepath)
{
  /* file name have to be shorter than MAX_FILENAME(1024) characters*/
  char filename[MAX_FILENAME];
  findFilename(filepath, filename);

  off_t fileLength;
  fileLength = getFileLength(filepath);
  if (fileLength < 0) errorExit("getting file length failed");

  if (request->requireRange == REQUIRE_RANGE_FALSE)
  {
    sprintf(header,
            "HTTP/1.1 200 OK\r\n"
            "Accept-Ranges: bytes\r\n"
            "Content-Disposition: attachment; filename=\"%s\"\r\n"
            "Content-Length: %lld\r\n"
            "\r\n", filename, fileLength);
  } else if (request->requireRange == REQUIRE_RANGE_TRUE)
  {
    sprintf(header,
            "HTTP/1.1 206 Partial\r\n"
            "Content-Disposition: attachment; filename=\"%s\"\r\n"
            "Content-Range: bytes %lld-%lld/%lld\r\n"
            "Content-Length: %lld\r\n"
            "Content-Type: multipart/byteranges\n"
            "\r\n", filename, request->offset, request->end, fileLength, request->end - request->offset);
    printf("offset: %lld\nend: %lld\n", request->offset, request->end);
  }
  puts(header);
}

#ifdef __APPLE__
void sendFile(char *filepath, int clientSocketFD, httpRquest *request)
{
  FILE *file = fopen(filepath, "rb");
  if (!file) errorExit("failed to open file");

  if (request->requireRange == REQUIRE_RANGE_FALSE)
    request->end = getFileLength(filepath);

  off_t totalSize = request->end - request->offset;
  off_t offset = request->offset;
  off_t totalBytesSent = 0;
  off_t len = BUFFER_SIZE;
  int ret;

  /* send file in chunks */
  while (totalBytesSent < totalSize)
  {
    if ((totalSize - totalBytesSent) < BUFFER_SIZE)
      len = totalSize - totalBytesSent;

    ret = sendfile(fileno(file), clientSocketFD, offset, &len, NULL, 0);
    if (ret < 0) errorExit("failed to send file");
    totalBytesSent += len;
    offset += len;
  }
}
#endif

#ifdef __linux__
void sendFile(char *filepath, int clientSocketFD, httpRquest *request)
{
  FILE *file = fopen(filepath, "rb");
  if (!file) errorExit("failed to open file");

  off_t bytesLeftToSend = request->end - request->offset;
  off_t offset = request->offset;
  off_t bytesSentInOneAction = 0;
  off_t len = BUFFER_SIZE;
  while (bytesLeftToSend > 0)
  {
    if (bytesLeftToSend < BUFFER_SIZE) len = bytesLeftToSend;
    bytesSentInOneAction = sendfile(clientSocketFD, fileno(file), &offset, len);
    if (bytesSentInOneAction < 0) errorExit("failed to send file");
    bytesLeftToSend -= bytesSentInOneAction;
    offset += bytesSentInOneAction;
  }
}
#endif

void serveFile(int clientSocketFD, char *filepath)
{
  httpRquest request;

  readHeaderFromClient(clientSocketFD, &request);

  char header[BUFFER_SIZE];
  composeHeader(header, &request, filepath);
  write(clientSocketFD, header, strlen(header));

  sendFile(filepath, clientSocketFD, &request);
}

void initListening(int socketFD, struct sockaddr_in *address, int port)
{
  /* create socket */
  if (socketFD < 0)
    errorExit("creating socket failed");

  /* configure socket */

  address->sin_family = AF_INET;
  address->sin_port = htons(port); /* host to network short */
  address->sin_addr.s_addr = INADDR_ANY;

  /* bind & listen */
  if(bind(socketFD, (struct sockaddr*) address, sizeof(*address)) != 0)
    errorExit("binding failed");

  if (listen(socketFD, 16)!=0) errorExit("listening failed");
}

void serve(char* filepath, int port)
{
  int socketFD = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in address;
  initListening(socketFD, &address, port);

  /* accept client */
  for (;;)
  {
    socklen_t size = sizeof(address);
    int clientSocketFD = accept(socketFD, (struct sockaddr*) &address, &size);

    puts("client connected");

    if (fork() == 0)
    {
      serveFile(clientSocketFD, filepath);
      exit(0);
    }
    else
    {
      close(clientSocketFD);
    }
  }
}

int main(int argc, char *argv[])
{
  if (strcmp(argv[1], "-h") == 0)
  {
    puts("format: pass <filepath> <port>");
    puts("file name(not path) must not exceed 1024 characters");
    puts("keyboard interrupt to kill");
    return 0;
  }

  if (argc < 3 || argc > 3)
  {
    fprintf(stderr, "needs 2 arguments: file path & port\n");
    return -1;
  }

  char *filepath = argv[1];
  int port = atoi(argv[2]);

  printf("serving  %s  on port  %d\n"
         "keyboard interrupt to kill\n", filepath, port);

  expandFilePath(filepath);
  serve(filepath, port);
}
