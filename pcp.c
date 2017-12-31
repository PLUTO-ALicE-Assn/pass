#include <pcp-client/pcp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include <ifaddrs.h>
#include <stdio.h>
#include <string.h>

/*  get machine's own sddress
 *  http://man7.org/linux/man-pages/man3/getifaddrs.3.html
 *  interface can be en0, lo0, etc
 *  ipVersion can be AF_INET or AF_INET6
 */

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

    return address;
  }

  freeifaddrs(ifaddrHead);

  return 0;
}

int main()
{
  /* get port & address */
  const int port = 8765;
  long int address = getInternalAddress((char*) &"en0", AF_INET);

  if (!address)
  {
    fprintf(stderr, "get internal address error");
    return -1;
  }

  /* init pcp */
  pcp_ctx_t *pcpCXT = pcp_init(ENABLE_AUTODISCOVERY, NULL); /* 2nd argument should be optional but not */

  /* configure internal address */
  struct sockaddr_in sockaddress;
  sockaddress.sin_family = AF_INET;
  sockaddress.sin_port = htons(port); /* host to network short */
  sockaddress.sin_addr.s_addr = (in_addr_t) address;

  /* start pcp, 3600 is lifetime in seconds */
  pcp_flow_t *flow = pcp_new_flow(pcpCXT, (struct sockaddr*) &sockaddress, NULL, NULL, AF_INET, 3600, NULL);

  /* get external address & port */
  size_t infoCount = 1024;
  pcp_flow_info_t *info = pcp_flow_get_info(flow, &infoCount);
  struct in6_addr externalAddress = info->ext_ip;
  uint16_t externalPort = info->ext_port;

  /* convert numeric to string */
  char *externalAddressStr = (char*) malloc(INET6_ADDRSTRLEN);
  inet_ntop(AF_INET6, &externalAddress, externalAddressStr, INET6_ADDRSTRLEN);

  printf("\n++++++++\nTell Yuan what is this: %s\n%d++++++++\n\n", externalAddressStr, externalPort);

  free(externalAddressStr);
  free(info);
  pcp_close_flow(flow);
  pcp_delete_flow(flow);
  pcp_terminate(pcpCXT, 1);

  return 0;
}
