/* #include <miniupnpc/miniupnpc.h> */
/* #include <miniupnpc/upnpcommands.h> */

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "miniupnpc/miniupnpc.h"
#include "miniupnpc/upnpcommands.h"
#include "log.h"

#define DELAY_TIME 5000 /* in milliseconds. set it longer because user needs to click allow if there is a firewall */
#define DEFAULT_TTL 2
#define PROTOCOL "TCP"
#define DEFAULT_LEASE "3600"

void handleReturnCode (int code)
{
  switch (code)
  {
  case 0:
    log_info("SUCCESS");
    break;
  case 402:
    log_warn("Invalid Args - See UPnP Device Architecture section on Control.");
    exit(-1);
  case 501:
    log_warn("Action Failed - See UPnP Device Architecture section on Control.");
    exit(-1);
  case 606:
    log_warn("Action not authorized - The action requested REQUIRES authorization and the sender was not authorized.");
    exit(-1);
  case 715:
    log_warn("WildCardNotPermittedInSrcIP - The source IP address cannot be wild-carded");
    exit(-1);
  case 716:
    log_warn("WildCardNotPermittedInExtPort - The external port cannot be wild-carded ConflictInMappingEntry - The port mapping entry specified conflicts");
    exit(-1);
  case 718:
    log_warn("ConflictInMappingEntry - The port mapping entry specified conflicts with a mapping assigned previously to another client");
    exit(-1);
  case 724:
    log_warn("SamePortValuesRequired - Internal and External port values must be the same");
    exit(-1);
  case 725:
    log_warn("OnlyPermanentLeasesSupported - The NAT implementation only supports permanent lease times on port mappings");
    exit(-1);
  case 726:
    log_warn("RemoteHostOnlySupportsWildcard - RemoteHost must be a wildcard and cannot be a specific IP address or DNS name");
    exit(-1);
  case 727:
    log_warn("ExternalPortOnlySupportsWildcard - ExternalPort must be a wildcard and cannot be a specific port value");
    exit(-1);
  case 728:
    log_warn("NoPortMapsAvailable - There are not enough free ports available to complete port mapping.");
    exit(-1);
  case 729:
    log_warn("ConflictWithOtherMechanisms - Attempted port mapping is not allowed due to conflict with other mechanisms.");
    exit(-1);
  case 732:
    log_warn("WildCardNotPermittedInIntPort - The internal port cannot be wild-carded");
    exit(-1);
  default:
    log_warn("undefined error");
    exit(-1);
  }

}

/* externalAddress has to be at least 16 bit */
/* map a external port to a same internal port */
 /* and print out external address */
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
  UPNP_GetValidIGD(deviceList, &urls, &data, internalAddress, sizeof(internalAddress));

  char externalAddress[64];
  log_info("get external address");
  handleReturnCode(UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalAddress));


  char portStr[256];
  sprintf(portStr, "%d", port);

  log_info("map port");
  handleReturnCode(UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, portStr, portStr, internalAddress, NULL, PROTOCOL, NULL, "3600"));

  /* let user know the external address */
  printf("external address:\n%s:%d\n", externalAddress, port);
  printf("press enter to exit");

  char ch[64];
  scanf("%63s", ch);

  log_info("closing port ");
  handleReturnCode(UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, portStr, PROTOCOL, NULL));
  log_info("SUCCESS");
}

/* int main() */
/* { */
/*   mapPort(10086); */
/*   return 0; */
/* } */
