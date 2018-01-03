#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

#include "miniupnpc.h"
#include "upnpcommands.h"
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>


#define DELAY_TIME 5000 /* in milliseconds. set it longer because user needs to click allow if there is a firewall */
#define DEFAULT_TTL 2
#define PROTOCOL "TCP"
#define DEFAULT_LEASE "3600"

void handleReturnCode (int code)
{
  switch (code)
  {
  case 0:
    puts("SUCCESS");
    break;
  case 402:
    puts("Invalid Args - See UPnP Device Architecture section on Control.");
    break;
  case 501:
    puts("Action Failed - See UPnP Device Architecture section on Control.");
    break;
  case 606:
    puts("Action not authorized - The action requested REQUIRES authorization and the sender was not authorized.");
    break;
  case 715:
    puts("WildCardNotPermittedInSrcIP - The source IP address cannot be wild-carded");
    break;
  case 716:
    puts("WildCardNotPermittedInExtPort - The external port cannot be wild-carded ConflictInMappingEntry - The port mapping entry specified conflicts");
    break;
  case 718:
    puts("ConflictInMappingEntry - The port mapping entry specified conflicts with a mapping assigned previously to another client");
    break;
  case 724:
    puts("SamePortValuesRequired - Internal and External port values must be the same");
    break;
  case 725:
    puts("OnlyPermanentLeasesSupported - The NAT implementation only supports permanent lease times on port mappings");
    break;
  case 726:
    puts("RemoteHostOnlySupportsWildcard - RemoteHost must be a wildcard and cannot be a specific IP address or DNS name");
    break;
  case 727:
    puts("ExternalPortOnlySupportsWildcard - ExternalPort must be a wildcard and cannot be a specific port value");
    break;
  case 728:
    puts("NoPortMapsAvailable - There are not enough free ports available to complete port mapping.");
    break;
  case 729:
    puts("ConflictWithOtherMechanisms - Attempted port mapping is not allowed due to conflict with other mechanisms.");
    break;
  case 732:
    puts("WildCardNotPermittedInIntPort - The internal port cannot be wild-carded");
    break;
  }

}

/* externalAddress has to be at least 16 bit */
void mapPort(int port)
{
  struct UPNPDev * deviceList;
  const char * multicastInterface = NULL;
  const char * mini_ssdpd_socket_fd = NULL;
  int ipv6 = 0;
  int error;

  deviceList = upnpDiscover(DELAY_TIME, multicastInterface, mini_ssdpd_socket_fd,
                         0/*sameport*/, ipv6, DEFAULT_TTL, &error);

  struct UPNPUrls urls;
  struct IGDdatas data;
  if (!deviceList) printf("upnpDiscover() error code=%d\n", error);

  char internalAddress[64];
  char externalAddress[64];
  UPNP_GetValidIGD(deviceList, &urls, &data, internalAddress, sizeof(internalAddress));

  printf("get external address ");
  handleReturnCode(UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalAddress));


  char portStr[256];
  sprintf(portStr, "%d", port);

  printf("map port ");
  handleReturnCode(UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, portStr, portStr, internalAddress, NULL, PROTOCOL, NULL, "3600"));

  /* if stdout connect to terminal, means in verbose mode */
  if (isatty(fileno(stdout)) == 1)
  printf("external address:\n%s:%d\n", externalAddress, port);
  else /* not in verbose mode */
  {
    freopen("/dev/tty", "w", stdout);
    printf("external address:\n%s:%d\n", externalAddress, port);
    freopen("log", "a+", stdout);
  }

  char ch[64];
  gets(ch);

  printf("close port ");
  handleReturnCode(UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, portStr, PROTOCOL, NULL));
}

/* int main() */
/* { */
/*   mapPort(10086); */
/*   return 0; */
/* } */
