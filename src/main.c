#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "upnp.h"

#define TRUE 1
#define FALSE 0

void serve(char* filepath, int port);
void expandFilePath(char* filepath);

long int getInternalAddress(char* interface, sa_family_t ipVersion)
{
  struct ifaddrs *ifaddrHead, *ifaddr;
  /* int_8 */
  sa_family_t family;
  int n;
  char *interfaceName;

  if (getifaddrs(&ifaddrHead) != 0)
    {
      fprintf(stderr, "ifaddrs error");
      return -1;
    }

  /* iterate through address list */
  for (ifaddr = ifaddrHead, n = 0; ifaddr != NULL; ifaddr = ifaddr->ifa_next, n++)
    {
      family = ifaddr->ifa_addr->sa_family;
      interfaceName = ifaddr->ifa_name;

      if (!family || family != ipVersion || strcmp(interfaceName, interface) != 0) continue;

      struct sockaddr *addr = ifaddr->ifa_addr;
      struct sockaddr_in* addr_in = (struct sockaddr_in*) addr;
      long int address = addr_in->sin_addr.s_addr;

      freeifaddrs(ifaddrHead);

      return htonl(address);
    }

  freeifaddrs(ifaddrHead);

  return 0;
}


int main(int argc, char *argv[])
{

  /* if wrong number of arguments are entered */
  if (argc < 3 || argc > 4)
    {
      fprintf(stderr, "needs 2/3 arguments: file path & port\n");
      return -1;
    }

  /* display help message */
  if (strcmp(argv[1], "-h") == 0)
    {
      puts("format: pass <filepath> <port>");
      puts("file name(not path) must not exceed 1024 characters");
      puts("keyboard interrupt to kill");
      return 0;
    }

  int map = FALSE;

  if (argc == 4)
  {
    if (strcmp(argv[3], "map") == 0)
    {
      map = TRUE;
    }
  }


  /* get file path and port */
  char *filepath = argv[1];
  int port;
  sscanf(argv[2], "%d", &port);

  pid_t pid;
  if ((pid = fork()) == 0)
  {
    serve(filepath, port);
  }
  else
  {
    upnp_flow flow;

    if (map) /* map port */
    {
      mapInit(&flow);
      mapPort(port, &flow);
      /* let user know the external address */
      printf("download link:\nhttp://%s:%d\n", flow.externalAddress, port);
    }
    else /* not map port */
    {
      long int address;
      if ((address = getInternalAddress("en0", AF_INET6)) < 0)
        {
          if ((address = getInternalAddress("en0", AF_INET)) < 0)
            fprintf(stderr, "can't get internal address");
        }

      char *addressStr = (char*) malloc(INET6_ADDRSTRLEN);
      inet_ntop(AF_INET6, &address, addressStr, INET6_ADDRSTRLEN);

      printf("download link:\nhttp://[%s]:%d\n", addressStr, port);
    }

    printf("press enter to exit\n");
    getchar();

    if (map) removeMapping(&flow);

    kill(pid, SIGTERM);
  }
}
