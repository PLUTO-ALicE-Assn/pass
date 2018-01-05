#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "log.h"

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
  if (argc < 3 || argc > 3)
    {
      fprintf(stderr, "needs 2/3 arguments: file path & port\n");
      return -1;
    }

  /* set log file */
  FILE *logFile = fopen("log", "a+");
  if (logFile == NULL) perror("open log file failed");
  log_set_fp(logFile);

  /* get file path and port */
  char *filepath = argv[1];
  int port;
  sscanf(argv[2], "%d", &port);

  pid_t pid;
  if ((pid = fork()) == 0)
  {
    log_info("expand file path");
    expandFilePath(filepath);
    log_info("serve file");
    serve(filepath, port);
  }
  else
  {
    log_info("maping port");
    mapPort(port);
    log_info("exiting");
    kill(pid, SIGTERM);
  }
}
