
#ifdef __linux__

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>

#include "hal_utils.h"


bool NetwHlpr_getInterfaceMACAddress(const char *iface, uint8_t *addr)
{
	if (iface == NULL || addr == NULL) return false;
	int sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock >= 0) {
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(struct ifreq));
		NetwHlpr_interfaceInfo(iface, NULL, (char *)ifr.ifr_name, NULL, NULL);
		ioctl(sock, SIOCGIFHWADDR, &ifr);
		close(sock);
		memcpy(addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
		return true;
	}
	return false;
}

static void anyIpAddrFromIface(const char *ifacename, char *ipaddr, struct ifaddrs *ifaddr)
{
	struct ifaddrs *it;
	if (!ifaddr) {
		if (getifaddrs(&ifaddr) != 0) return;
	}
	for (it = ifaddr; it != NULL; it = it->ifa_next)  {
		struct sockaddr_in *sa = (struct sockaddr_in *)it->ifa_addr;
		if (sa && sa->sin_family == AF_INET) {
			if (strcmp(ifacename, it->ifa_name) == 0) {
				Hal_ipv4BinToStr(htonl(sa->sin_addr.s_addr), ipaddr);
				break;
			}
		}
	}
	freeifaddrs(ifaddr);
}

bool NetwHlpr_interfaceInfo(const char *iface, int *num, char *name, char *name2, char *addr)
{
	if (!iface) return false;
	char buf[64];
	int idx = -1;
	const char *ifname = NULL;
	in_addr_t iaddr;

	if (strlen(iface) < (sizeof("0.0.0.0")-1)) {
		if (sscanf(iface, "%d", &idx) == 1) {
			if (addr || name || name2) {
				if_indextoname(idx, buf);
				ifname = buf;
				if (addr) anyIpAddrFromIface(ifname, addr, NULL);
			}
			goto exit_found;
		}
	}
	idx = if_nametoindex(iface);
	if (idx > 0) {
		ifname = iface;
		if (addr) anyIpAddrFromIface(ifname, addr, NULL);
		goto exit_found;
	}
	iaddr = inet_addr(iface);
	if (iaddr != INADDR_NONE) {
		struct ifaddrs *ifaddr, *it;
		if (getifaddrs(&ifaddr) != 0) return false;
		for (it = ifaddr; it != NULL; it = it->ifa_next)  {
			struct sockaddr_in *sa = (struct sockaddr_in *)it->ifa_addr;
			if (sa && sa->sin_addr.s_addr == iaddr) {
				strcpy(buf, it->ifa_name);
				idx = if_nametoindex(buf);
				ifname = buf;
				break;
			}
		}
		if (ifname && addr) anyIpAddrFromIface(ifname, addr, ifaddr);
		else freeifaddrs(ifaddr);
		if (ifname) goto exit_found;
	}

	return false;

exit_found:
	if (num) { *num = idx; }
	if (name) { strcpy(name, ifname); }
	if (name2) { strcpy(name2, ifname); }
	return true;
}

#endif // __linux__