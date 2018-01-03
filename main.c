#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

void mapPort(int port);
void serve(char* filepath, int port);
void expandFilePath(char* filepath);


int main(int argc, char *argv[])
{
  if (strcmp(argv[1], "-h") == 0)
    {
      puts("format: pass <filepath> <port>");
      puts("file name(not path) must not exceed 1024 characters");
      puts("keyboard interrupt to kill");
      return 0;
    }

  if (argc < 3 || argc > 4)
    {
      fprintf(stderr, "needs 2/3 arguments: file path & port\n");
      return -1;
    }

  char *filepath = argv[1];
  int port;
  sscanf(argv[2], "%d", &port);

  if (argc == 4)
  {
    if (strcmp(argv[3], "verbose") != 0)
    {
      puts("\"verbose\" for verbose mode");
      exit(-1);
    }
  }
  else freopen("log", "a+", stdout);


  pid_t pid;
  if ((pid = fork()) == 0)
  {
    expandFilePath(filepath);
    serve(filepath, port);
  }
  else
  {
    mapPort(port);
    kill(pid, SIGTERM);
  }


}
