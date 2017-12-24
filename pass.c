#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wordexp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h> /* for close() */

#define BUFFER_SIZE 1024

/* TODO: implement ranges
 * TODO: support multi-threading downloads
 * TODO: add error checks
 * TODO: graceful shutdown
 *
 */

/* find file name from path */
/* file name have to be shorter than 512 characters*/
/* remember to deallocate memory */
char *findFilename(char *filepath)
{
  size_t pt;
  size_t filenamePt = 0;
  char ch;
  char *filename = (char*) malloc(512);

  for (pt = 0; pt < strlen(filepath); pt++)
  {
    ch = filepath[pt];
    filename[filenamePt] = ch;
    filenamePt++;

    if(ch == '/')
    {
      bzero(filename, 512);
      filenamePt = 0;
    }
  }

  filename[filenamePt + 1] = '\0';

  return filename;
}

int expandFilePath(char* filepath)
{
  wordexp_t wordExpand;

  if (wordexp(filepath, &wordExpand, 0) != 0)
  {
    perror("expanding file path failed");
    return -1;
  }

  filepath = wordExpand.we_wordv[0];
  return 0;
}

int composeHeader(char* filepath, char* header)
{
  if (expandFilePath(filepath) != 0) return -1;

  /* get file naem & length and write to header */
  /* get file name */
  char *filename = findFilename(filepath);

  /* get file length */

  off_t fileLength;
  struct stat statBuffer;

  if (stat(filepath, &statBuffer) != 0 || (!S_ISREG(statBuffer.st_mode)))
  {
    perror("checking status of file failed, is it a regular file?");
    return -1;
  }
  fileLength = statBuffer.st_size;

  /* compose header */
  sprintf(header,
          "HTTP/1.1 200 OK\n"
          "Content-Length: %lli\n"
          "Accept-Ranges: none\n"
          "Content-Disposition: attachment; filename=\"%s\"\n"
          "\n", fileLength, filename);

  /* do not need filename anymore */
  free(filename);

  return 0;
}

int serveFile(char* filepath, int port)
{
  if (expandFilePath(filepath) != 0) return -1;

  /*
   *  prepare
   */

  char header[512];

  if (composeHeader(filepath, header) < 0)
  {
    fprintf(stderr, "header composition failed");
    return -1;
  }

  /*
   *  tranfer
   */

  /* create socket */
  int socketfd = socket(PF_INET, SOCK_STREAM, 0);
  if (socketfd < 0)
  {
    perror("creating socket failed");
    return -1;
  }

  /* configure socket */
  struct sockaddr_in address;

  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(port); /* host to network short */
  address.sin_addr.s_addr = INADDR_ANY;

  /* bind & listen */
  if(bind(socketfd, (struct sockaddr*) &address, sizeof(address)) != 0)
  {
    perror("binding failed");
    return -1;
  }

  if (listen(socketfd, 16)!=0)
  {
    perror("listening failed");
    return -1;
  }

  /* accept client */
  while (1)
  {
    socklen_t size = sizeof(address);
    int clientSocket = accept(socketfd, (struct sockaddr*) &address, &size);

    puts("client connected");

    if (fork() == 0)
    {
      /* send header */
      write(clientSocket, header, strlen(header));

      size_t bytesRead = 0;
      long int bytesWrote = 0; /* could be negative, write() return -1 on error */

      char buffer[BUFFER_SIZE];

      FILE *file = fopen(filepath, "rb");
      if (!file)
      {
        perror("opening file failed");
        return -1;
      }

      /* set file pointer back */
      fseek(file, 0, SEEK_SET);

      /* send file in chunks */
      while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
      {
        bytesWrote = write(clientSocket, buffer, BUFFER_SIZE);
        if (bytesWrote < 0)
        {
          perror("writing failed");
          fseek(file, 0, SEEK_SET);
          exit(-1);
        }
        /* if write() didn't write BUFFER_SIZE bytes, go back n bytes */
        /* TODO: test this. Works normally, but not sure when bytesWrote is not same as BUFFER_SIZE */
        fseek(file, (bytesWrote - BUFFER_SIZE), SEEK_CUR);
      }

      /* set file pointer back */
      fseek(file, 0, SEEK_SET);

      exit(0);
    }
    else
    {
      close(clientSocket);
    }
  }

  /* never close file  */

}


int main(int argc, char *argv[])
{
  if (strcmp(argv[1], "-h") == 0)
  {
    puts("format: pass <filepath> <port>");
    puts("file name(not path) must not exceed 512 characters");
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

  if (serveFile(filepath, port) < 0) return -1;
}
