#include "upnp.h"


void handleReturnCode (int code)
{
  switch (code)
  {
  case 0:
    break;
  case 402:
    puts("Invalid Args - See UPnP Device Architecture section on Control.");
    exit(-1);
  case 501:
    puts("Action Failed - See UPnP Device Architecture section on Control.");
    exit(-1);
  case 606:
    puts("Action not authorized - The action requested REQUIRES authorization and the sender was not authorized.");
    exit(-1);
  case 715:
    puts("WildCardNotPermittedInSrcIP - The source IP address cannot be wild-carded");
    exit(-1);
  case 716:
    puts("WildCardNotPermittedInExtPort - The external port cannot be wild-carded ConflictInMappingEntry - The port mapping entry specified conflicts");
    exit(-1);
  case 718:
    puts("ConflictInMappingEntry - The port mapping entry specified conflicts with a mapping assigned previously to another client");
    exit(-1);
  case 724:
    puts("SamePortValuesRequired - Internal and External port values must be the same");
    exit(-1);
  case 725:
    puts("OnlyPermanentLeasesSupported - The NAT implementation only supports permanent lease times on port mappings");
    exit(-1);
  case 726:
    puts("RemoteHostOnlySupportsWildcard - RemoteHost must be a wildcard and cannot be a specific IP address or DNS name");
    exit(-1);
  case 727:
    puts("ExternalPortOnlySupportsWildcard - ExternalPort must be a wildcard and cannot be a specific port value");
    exit(-1);
  case 728:
    puts("NoPortMapsAvailable - There are not enough free ports available to complete port mapping.");
    exit(-1);
  case 729:
    puts("ConflictWithOtherMechanisms - Attempted port mapping is not allowed due to conflict with other mechanisms.");
    exit(-1);
  case 732:
    puts("WildCardNotPermittedInIntPort - The internal port cannot be wild-carded");
    exit(-1);
  default:
    puts("undefined error");
    exit(-1);
  }
}


/* externalAddress has to be at least 16 bit */
/* map a external port to a same internal port */
 /* and print out external address */
void mapInit(upnp_flow *flow)
{
  struct UPNPDev * deviceList;
  const char * multicastInterface = NULL;
  const char * mini_ssdpd_socket_fd = NULL;
  int ipv6 = 0;
  int error;

  deviceList = upnpDiscover(DELAY_TIME, multicastInterface, mini_ssdpd_socket_fd,
                         0/*sameport*/, ipv6, DEFAULT_TTL, &error);

  struct UPNPUrls *urls = &flow->urls;
  struct IGDdatas *data = &flow->data;
  if (!deviceList)
  {
    printf("upnpDiscover() error code=%d\n", error);
    exit(-1);
  }



  UPNP_GetValidIGD(deviceList, urls, data, flow->internalAddress, sizeof(flow->internalAddress));

  char *externalAddress = flow->externalAddress;
  handleReturnCode(UPNP_GetExternalIPAddress(urls->controlURL, data->first.servicetype, externalAddress));

  }


void mapPort(int port, upnp_flow *flow)
{
  char *portStr = flow->port;
  sprintf(portStr, "%d", port);

  struct UPNPUrls *urls = &flow->urls;
  struct IGDdatas *data = &flow->data;

  char *internalAddress = flow->internalAddress;
  handleReturnCode(UPNP_AddPortMapping(urls->controlURL, data->first.servicetype, portStr, portStr, internalAddress, NULL, PROTOCOL, NULL, "3600"));
}

void removeMapping(upnp_flow *flow)
{
    struct UPNPUrls *urls = &flow->urls;
    struct IGDdatas *data = &flow->data;
    char *portStr = flow->port;

    handleReturnCode(UPNP_DeletePortMapping(urls->controlURL, data->first.servicetype, portStr, PROTOCOL, NULL));
}

/* int main() */
/* { */
/*   int port = 9090; */
/*   upnp_flow flow; */
/*   mapInit(&flow); */
/*   mapPort(port, &flow); */
/*   /\* let user know the external address *\/ */
/*   printf("download link:\nhttp://%s:%d\n", flow.externalAddress, port); */

/*   printf("press enter to exit"); */
/*   getchar(); */
/*   return 0; */
/* } */
