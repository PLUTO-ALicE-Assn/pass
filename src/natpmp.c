#include <stdio.h>
#include <string.h>
#include "natpmp.h"

natpmp_t redirect(uint16_t privateport, uint16_t publicport)
{
  int r;
  natpmp_t natpmp;
  natpmpresp_t response;
  initnatpmp(&natpmp, 0, 0);
  sendnewportmappingrequest(&natpmp, NATPMP_PROTOCOL_TCP, privateport, publicport, 3600);
  do {
    fd_set fds;
    struct timeval timeout;
    FD_ZERO(&fds);
    FD_SET(natpmp.s, &fds);
    getnatpmprequesttimeout(&natpmp, &timeout);
    select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
    r = readnatpmpresponseorretry(&natpmp, &response);
  } while(r==NATPMP_TRYAGAIN);
  printf("mapped public port %hu to localport %hu liftime %u\n",
         response.pnu.newportmapping.mappedpublicport,
         response.pnu.newportmapping.privateport,
         response.pnu.newportmapping.lifetime);
  return natpmp;
}

int main ()
{
  natpmp_t natpmp = redirect(0, 0);
  char cmd;
  gets(&cmd);
  if (strncmp(&cmd, "q", 1))
    closenatpmp(&natpmp);
}
