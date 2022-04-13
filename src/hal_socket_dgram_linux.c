
#ifdef __linux__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "hal_socket_dgram.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>


struct sDgramSocket {
	int fd;
	int domain;
	union uDgramSocketAddress remote;
	int ifidx;
	int protocol;
};


static bool prepareSocketAddress(const char *address, uint16_t port, struct sockaddr_in *sockaddr);
static char *inet_paddr(in_addr_t in, char *out);


static inline void setSocketNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static inline int getSocketAvailableToRead(int fd)
{
    int val = 0;
	if (ioctl(fd, FIONREAD, &val) == 0) {
		return val;
	}
	return -1;
}


DgramSocket UdpDgramSocket_createAndBind(const char *ip, uint16_t port)
{
	DgramSocket self;
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return NULL;

	if (ip || port) {
		struct sockaddr_in addr;
		if (!prepareSocketAddress(ip, port, &addr)) {
			goto exit_error;
		}
		if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
			goto exit_error;
		}
	}

	self = (DgramSocket)calloc(1, sizeof(struct sDgramSocket));
	if (self) {
		self->fd = fd;
		self->domain = AF_INET;
		self->protocol = -1;
		setSocketNonBlocking(fd);
		return self;
	}

exit_error:
	close(fd);
	return NULL;
}

DgramSocket UdpDgramSocket_create(void)
{
	return UdpDgramSocket_createAndBind(0, 0);
}

bool UdpDgramSocket_bind(DgramSocket self, const char *ip, uint16_t port)
{
	if (self == NULL) return false;
	struct sockaddr_in addr;
	if (!prepareSocketAddress(ip, port, &addr)) {
		return false;
	}
	if (bind(self->fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
		return false;
	}
	return true;
}

bool UdpDgramSocket_joinGroup(DgramSocket self, const char *ip, const char *iface)
{
	if (self == NULL || ip == NULL || iface == NULL) return false;
	struct ip_mreqn imreqn;
	struct ifreq ifr;
	imreqn.imr_multiaddr.s_addr = inet_addr(ip);
	imreqn.imr_address.s_addr = htonl(INADDR_ANY);
	imreqn.imr_ifindex = if_nametoindex(iface);
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, iface);
	if (setsockopt(self->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreqn, sizeof(struct ip_mreqn)) < 0) {
		return false;
	}
	if (setsockopt(self->fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(struct ifreq)) < 0) {
		return false;
	}
	return true;
}

bool UdpDgramSocket_setReuse(DgramSocket self, bool reuse)
{
	if (self == NULL) return false;
	int sockopt = (reuse)? 1 : 0;
	if (setsockopt(self->fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) < 0) {
		return false;
	}
	return true;
}


DgramSocket LocalDgramSocket_create(const char *address)
{
	if (address == NULL) return NULL;

	int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) return NULL;

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, address);
	DgramSocket self;

	if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
		goto exit_error;
	}

	self = (DgramSocket)calloc(1, sizeof(struct sDgramSocket));
	if (self) {
		self->fd = fd;
		self->domain = AF_UNIX;
		self->protocol = -1;
		setSocketNonBlocking(fd);
		return self;
	}

exit_error:
	close(fd);
	return NULL;
}


void LocalDgramSocket_unlinkAddress(const char *address)
{
	unlink(address);
}


DgramSocket EtherDgramSocket_create(const char *iface, uint16_t ethTypeFilter)
{
	if (iface == NULL) return NULL;

	if (ethTypeFilter == 0) ethTypeFilter = ETH_P_ALL;
	ethTypeFilter = htons(ethTypeFilter);

	int fd = socket(AF_PACKET, SOCK_RAW, ethTypeFilter);
	if (fd < 0) return NULL;

	int idx;
	int sockopt;
	struct packet_mreq mreq;
	DgramSocket self;

	bzero(&mreq, sizeof(struct packet_mreq));

	idx = (int)if_nametoindex(iface);

	mreq.mr_ifindex = idx;
	mreq.mr_type = PACKET_MR_PROMISC;

	sockopt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) < 0) goto exit_error;
	if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, iface, IFNAMSIZ-1) < 0) goto exit_error;
	if (setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) goto exit_error;

	self = (DgramSocket)calloc(1, sizeof(struct sDgramSocket));
	if (self) {
		self->fd = fd;
		self->domain = AF_PACKET;
		self->ifidx = idx;
		self->protocol = ethTypeFilter;
		setSocketNonBlocking(fd);
		return self;
	}

exit_error:
	close(fd);
	return NULL;
}

int EtherDgramSocket_setHeader(uint8_t *header, const uint8_t *src, const uint8_t *dst, uint16_t ethType)
{
	if (header == NULL || src == NULL || dst == NULL) return -1;
	memcpy(header, dst, ETH_ALEN);
	header += ETH_ALEN;
	memcpy(header, src, ETH_ALEN);
	header += ETH_ALEN;
	*((uint16_t*)header) = ntohs(ethType);
	header += 2;
	return 14;
}

void EtherDgramSocket_getHeader(const uint8_t *header, uint8_t *src, uint8_t *dst, uint16_t *ethType)
{
	if (header == NULL || src == NULL || dst == NULL || ethType == NULL) return;
	memcpy(dst, header, ETH_ALEN);
	header += ETH_ALEN;
	memcpy(src, header, ETH_ALEN);
	header += ETH_ALEN;
	*ethType = ntohs( *((uint16_t*)header) );
	header += 2;
}

bool EtherDgramSocket_getInterfaceMACAddress(const char *iface, uint8_t *addr)
{
	if (iface == NULL || addr == NULL) return false;
	int sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock >= 0) {
		struct ifreq ifr;
		bzero(&ifr, sizeof(struct ifreq));
		strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);
		ioctl(sock, SIOCGIFHWADDR, &ifr);
		close(sock);
		memcpy(addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
		return true;
	}
	return false;
}

uint8_t *EtherDgramSocket_getSocketMACAddress(DgramSocket self, uint8_t *addr)
{
	if (self == NULL || addr == NULL) return NULL;
	if (self->fd >= 0) {
		struct ifreq iface;
		bzero(&iface, sizeof(struct ifreq));
		if_indextoname(self->ifidx, iface.ifr_name);
		ioctl(self->fd, SIOCGIFHWADDR, &iface);
		memcpy(addr, iface.ifr_hwaddr.sa_data, ETH_ALEN);
		return addr;
	}
	return NULL;
}


bool DgramSocket_reset(DgramSocket self)
{
	if (self == NULL) return false;
	if (self->domain != AF_INET) return false;
	if (self->fd >= 0) {
		close(self->fd);
		uint16_t protocol = (self->protocol != -1)? (uint16_t)self->protocol : 0;
		self->fd = socket(self->domain, SOCK_DGRAM, protocol);
		if (self->fd >= 0) {
			return true;
		}
	}
	return false;
}

void DgramSocket_close(DgramSocket self) // hal internal use only
{
	if (self == NULL) return;
	if (self->fd >= 0) {
		close(self->fd);
		self->fd = -1;
	}
}

void DgramSocket_setRemote(DgramSocket self, const DgramSocketAddress addr)
{
	if (self == NULL || addr == NULL) return;
	memcpy(&self->remote, addr, sizeof(union uDgramSocketAddress));
}

void DgramSocket_getRemote(DgramSocket self, DgramSocketAddress addr)
{
	if (self == NULL || addr == NULL) return;
	memcpy(addr, &self->remote, sizeof(union uDgramSocketAddress));
}

static int socketReadFrom(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size, int flags)
{
	struct sockaddr_storage saddr;
	socklen_t addr_size;
	memset(&saddr, 0, sizeof(struct sockaddr_storage));
	switch (self->domain) {
		case AF_INET: {
			addr_size = sizeof(struct sockaddr_in);
		} break;
		case AF_UNIX: {
			addr_size = sizeof(struct sockaddr_un);
		} break;
		case AF_PACKET: {
			addr_size = sizeof(struct sockaddr_ll);
		} break;
		default: break;
	}
	int rc = recvfrom(self->fd, buf, size, flags, (struct sockaddr *)&saddr, &addr_size);
	if (rc > 0) {
		if (addr) {
			switch (self->domain) {
				case AF_INET: {
					struct sockaddr_in *paddr = (struct sockaddr_in *)&saddr;
					inet_paddr(htonl(paddr->sin_addr.s_addr), addr->ip);
					addr->port = htons(paddr->sin_port);
				} break;
				case AF_UNIX: {
					struct sockaddr_un *paddr = (struct sockaddr_un *)&saddr;
					strcpy(addr->address, paddr->sun_path);
				} break;
				case AF_PACKET: {
					struct sockaddr_ll *paddr = (struct sockaddr_ll *)&saddr;
					memcpy(addr->mac, paddr->sll_addr, ETH_ALEN);
				} break;
				default: break;
			}
		}
	}
	return rc;
}

int DgramSocket_readFrom(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size)
{
	if (self == NULL || addr == NULL || buf == NULL) return -1;
	return socketReadFrom(self, addr, buf, size, 0);
}

int DgramSocket_writeTo(DgramSocket self, const DgramSocketAddress addr, const uint8_t *buf, int size)
{
	if (self == NULL || addr == NULL || buf == NULL) return -1;
	struct sockaddr_storage saddr;
	socklen_t addr_size = 0;
	memset(&saddr, 0, sizeof(struct sockaddr_storage));
	switch (self->domain) {
		case AF_INET: {
			struct sockaddr_in *paddr = (struct sockaddr_in *)&saddr;
			addr_size = sizeof(struct sockaddr_in);
			paddr->sin_addr.s_addr = inet_addr(addr->ip);
			paddr->sin_family = AF_INET;
			paddr->sin_port = htons(addr->port);
		} break;
		case AF_UNIX: {
			struct sockaddr_un *paddr = (struct sockaddr_un *)&saddr;
			addr_size = sizeof(struct sockaddr_un);
			paddr->sun_family = AF_UNIX;
			strcpy(paddr->sun_path, addr->address);
		} break;
		case AF_PACKET: {
			const uint8_t *src = buf+6;
			struct sockaddr_ll *paddr = (struct sockaddr_ll *)&saddr;
			addr_size = sizeof(struct sockaddr_ll);
			paddr->sll_ifindex = self->ifidx;
			paddr->sll_halen = ETH_ALEN;
			memcpy(paddr->sll_addr, src, ETH_ALEN);
		} break;
		default: break;
	}
	return sendto(self->fd, buf, size, 0, (const struct sockaddr *)&saddr, addr_size);
}

int DgramSocket_read(DgramSocket self, uint8_t *buf, int size)
{
	if (self == NULL || buf == NULL) return -1;
	int rc = DgramSocket_readAvailable(self, true);
	if (rc <= 0) return rc;
	return socketReadFrom(self, NULL, buf, size, 0);
}

int DgramSocket_write(DgramSocket self, const uint8_t *buf, int size)
{
	if (self == NULL || buf == NULL) return -1;
	return DgramSocket_writeTo(self, &self->remote, buf, size);
}

int DgramSocket_readAvailable(DgramSocket self, bool fromRemote)
{
	if (self == NULL) return -1;

	uint8_t buf[1];
	union uDgramSocketAddress source;
	int ret, rc;
	while (1) {
		ret = getSocketAvailableToRead(self->fd);
		if (ret <= 0) return ret;
		if (!fromRemote) return ret;
		//
		rc = socketReadFrom(self, &source, buf, 1, MSG_PEEK);
		if (rc > 0) {
			switch (self->domain) {
				case AF_INET: {
					if (source.port == self->remote.port && strcmp(source.ip, self->remote.ip) == 0) {
						return ret;
					}
				} break;
				case AF_UNIX: {
					if (strcmp(source.address, self->remote.address) == 0) {
						return ret;
					}
				} break;
				case AF_PACKET: {
					if (memcmp(source.mac, self->remote.mac, ETH_ALEN) == 0) {
						return ret;
					}
				} break;
				default: break;
			}
			socketReadFrom(self, &source, buf, 1, 0); // flush
		} else {
			return -1;
		}
	}
}

void DgramSocket_destroy(DgramSocket self)
{
	if (self == NULL) return;
	DgramSocket_close(self);
	free(self);
}

unidesc DgramSocket_getDescriptor(DgramSocket self)
{
	if (self) {
		unidesc ret;
		ret.i32 = self->fd;
		return ret;
	}
	return Hal_getInvalidUnidesc();
}


static bool prepareSocketAddress(const char *address, uint16_t port, struct sockaddr_in *sockaddr)
{
	bool retVal = true;

	memset((char *)sockaddr, 0, sizeof(struct sockaddr_in));

	if (address != NULL) {
		struct addrinfo addressHints;
		struct addrinfo *lookupResult;
		int result;

		memset(&addressHints, 0, sizeof(struct addrinfo));
		addressHints.ai_family = AF_INET;
		result = getaddrinfo(address, NULL, &addressHints, &lookupResult);

		if (result != 0) {
			return false;
		}

		memcpy(sockaddr, lookupResult->ai_addr, sizeof(struct sockaddr_in));
		freeaddrinfo(lookupResult);
	} else {
		sockaddr->sin_addr.s_addr = htonl(INADDR_ANY);
	}

	sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons(port);

	return retVal;
}

static char *inet_paddr(in_addr_t in, char *out)
{
	char *ret = 0;
	union { uint32_t u32; uint8_t b[4]; } addr;
	addr.u32 = in;
	if ( sprintf(out, "%hhu.%hhu.%hhu.%hhu",
			addr.b[3], addr.b[2], addr.b[1], addr.b[0]) == 4 )
	{
		ret = out;
	}
	return ret;
}


#endif // __linux__