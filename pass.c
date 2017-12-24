#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>

#include <string.h>

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

int serveFile(char* filepath, int port)
{
  FILE *file;
  long int fileLength;

  /* get file name */
  char* filename = findFilename(filepath);
  if (!filename)
  {
    fprintf(stderr, "file path error\n");
    return -1;
  }

  /* open file */
  file = fopen(filepath, "rb");
  if (!file)
  {
    fprintf(stderr, "file open error\n");
    return -1;
  }

  /* get file length */
  fseek(file, 0, SEEK_END);
  fileLength=ftell(file);
  fseek(file, 0, SEEK_SET);


  /* compose header */
  char header[1024];

  sprintf(header,
          "HTTP/1.1 200 OK\n"
          "Content-Length: %li\n"
          "Content-Disposition: attachment; filename=\"%s\"\n"
          "\n", fileLength, filename);

  /* do not need filename anymore */
  free(filename);

  /* create socket */
  int socketfd = socket(PF_INET, SOCK_STREAM, 0);

  /* configure socket */
  struct sockaddr_in address;

  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(port); /* host to network short */
  address.sin_addr.s_addr = INADDR_ANY;

  /* bind & listen */
  if(bind(socketfd, (struct sockaddr*) &address, sizeof(address)) != 0)
  {
    fprintf(stderr, "bind error\n");
    return -1;
  }

  if (listen(socketfd, 16)!=0)
  {
    fprintf(stderr, "listen error\n");
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
      char buffer[BUFFER_SIZE];

      /* send file in chunks */
      while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
      {
        write(clientSocket, buffer, BUFFER_SIZE);
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
  serveFile(filepath, port);
}
