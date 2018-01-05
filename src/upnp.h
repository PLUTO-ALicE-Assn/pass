#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "miniupnpc/miniupnpc.h"
#include "miniupnpc/upnpcommands.h"

#define DELAY_TIME 5000 /* in milliseconds. set it longer because user needs to click allow if there is a firewall */
#define DEFAULT_TTL 2
#define PROTOCOL "TCP"
#define DEFAULT_LEASE "0" /* permanent */

typedef struct
{
  struct UPNPUrls urls;
  struct IGDdatas data;
  char port[64];
  char internalAddress[64];
  char externalAddress[64];
} upnp_flow;

void mapInit(upnp_flow *flow);
void mapPort(int port, upnp_flow *flow);
void removeMapping(upnp_flow *flow);
long int getInternalAddress(char* interface, sa_family_t ipVersion);
