#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0

void mapPort(int port);
void serve(char* filepath, int port);
void expandFilePath(char* filepath);


int main(int argc, char *argv[])
{
  /* display help message */
  if (strcmp(argv[1], "-h") == 0)
    {
      puts("format: pass <filepath> <port>");
      puts("file name(not path) must not exceed 1024 characters");
      puts("keyboard interrupt to kill");
      return 0;
    }

  /* if wrong number of arguments are entered */
  if (argc < 3 || argc > 4)
    {
      fprintf(stderr, "needs 2/3 arguments: file path & port\n");
      return -1;
    }

  int map = FALSE;

  if (argc == 4)
  {
    if (strcmp(argv[3], "map") == 0)
    {
      int map = TRUE;
    }
  }


  /* get file path and port */
  char *filepath = argv[1];
  int port;
  sscanf(argv[2], "%d", &port);

  pid_t pid;
  if ((pid = fork()) == 0)
  {
    expandFilePath(filepath);
    serve(filepath, port);
  }
  else
  {
    if (map) mapPort(port);

    printf("press enter to exit");
    getchar();

    kill(pid, SIGTERM);
  }
}
