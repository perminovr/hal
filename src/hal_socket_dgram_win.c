
#if defined(_WIN32) || defined(_WIN64)

#include "hal_socket_dgram.h"
#include "hal_syshelper.h"
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <netioapi.h>
#include <iphlpapi.h>
#include <windows.h>

#define ETH_ALEN	6
#define AF_PACKET	17 // yeah, netbios...
#define MIB_IPADDRTABLE_SZ_MAX (sizeof(DWORD) + sizeof(MIB_IPADDRROW)*128)


struct sDgramSocket {
	SOCKET s;
	int domain;
	union uDgramSocketAddress remote;
	int ifidx;
	int protocol;
};


static bool prepareSocketAddress(const char *address, uint16_t port, struct sockaddr_in *sockaddr);
static char *inet_paddr(uint32_t in, char *out);
static uint32_t getAnyIpFromInterface(ULONG idx);


static inline void setSocketNonBlocking(SOCKET s)
{
	u_long val = 1;
	ioctlsocket(s, FIONBIO, &val);
}

static inline int getSocketAvailableToRead(SOCKET s)
{
    u_long val = 0;
	if (ioctlsocket(s, FIONREAD, &val) == 0) {
		return (int)val;
	}
	return -1;
}


DgramSocket UdpDgramSocket_createAndBind(const char *ip, uint16_t port)
{
	HalShSys_init();

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) return NULL;

	if (ip || port) {
		struct sockaddr_in addr;
		if (!prepareSocketAddress(ip, port, &addr)) {
			goto exit_error;
		}
		if (bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) != 0) {
			goto exit_error;
		}
	}

	DgramSocket self = (DgramSocket)calloc(1, sizeof(struct sDgramSocket));
	if (self) {
		self->s = sock;
		self->domain = AF_INET;
		self->protocol = -1;
		setSocketNonBlocking(sock);
		return self;
	}

exit_error:
	closesocket(sock);
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
	if (bind(self->s, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) != 0) {
		return false;
	}
	return true;
}

bool UdpDgramSocket_joinGroup(DgramSocket self, const char *ip, const char *iface)
{
	if (self == NULL || ip == NULL || iface == NULL) return false;
	struct ip_mreq imreqn;
	ULONG idx = if_nametoindex(iface);
	inet_pton(AF_INET, ip, &imreqn.imr_multiaddr.s_addr);
	if (idx != 0) {
		imreqn.imr_interface.s_addr = getAnyIpFromInterface(idx);
	} else {
		inet_pton(AF_INET, iface, &imreqn.imr_interface.s_addr); // ip addr actually
	}
	if (setsockopt(self->s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&imreqn, sizeof(struct ip_mreq)) != 0) {
		return false;
	}
	return true;
}

bool UdpDgramSocket_setReuse(DgramSocket self, bool reuse)
{
	if (self == NULL) return false;
	int sockopt = (reuse)? 1 : 0;
	if (setsockopt(self->s, SOL_SOCKET, SO_REUSEADDR, (const char *)&sockopt, sizeof(sockopt)) != 0) {
		return false;
	}
	return true;
}


DgramSocket LocalDgramSocket_create(const char *address)
{
	if (address == NULL) return NULL;

	uint16_t port = Hal_generatePort(address, HAL_LOCAL_SOCK_PORT_MIN, HAL_LOCAL_SOCK_PORT_MAX);
	DgramSocket self = UdpDgramSocket_createAndBind(HAL_LOCAL_SOCK_ADDR, port);

	if (self) {
		self->domain = AF_UNIX;
	}

	return NULL;
}


void LocalDgramSocket_unlinkAddress(const char *address)
{
	DeleteFileA(address);
}


DgramSocket EtherDgramSocket_create(const char *iface, uint16_t ethTypeFilter)
{
	if (iface == NULL) return NULL;

	HalShSys_init();

	SOCKET sock = socket(AF_INET, SOCK_RAW, ethTypeFilter);
	if (sock != INVALID_SOCKET) return NULL;

	// ULONG idx = if_nametoindex(iface);
	// if (idx != 0) {
	// 	imreqn.imr_interface.s_addr = getAnyIpFromInterface(idx);
	// } else {
	// 	imreqn.imr_interface.s_addr = inet_addr(iface); // ip addr actually
	// }

	// int idx;
	// int sockopt;
	// struct packet_mreq mreq;

	// bzero(&mreq, sizeof(struct packet_mreq));

	// idx = (int)if_nametoindex(iface);

	// mreq.mr_ifindex = idx;
	// mreq.mr_type = PACKET_MR_PROMISC;

	// sockopt = 1;
	// if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&sockopt, sizeof(sockopt)) != 0) goto exit_error;
	// if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, iface, IFNAMSIZ-1) != 0) goto exit_error;
	// if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) != 0) goto exit_error;

	// DgramSocket self = (DgramSocket)calloc(1, sizeof(struct sDgramSocket));
	// if (self) {
	// 	self->s = sock;
	// 	self->domain = AF_PACKET;
	// 	self->protocol = ethTypeFilter;
	// 	setSocketNonBlocking(sock);
	// 	return self;
	// }

// exit_error:
	closesocket(sock);
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

	union {
		MIB_IPADDRTABLE t;
		char max[MIB_IPADDRTABLE_SZ_MAX];
	} u;
	ULONG sz = sizeof(u);
	MIB_IPADDRROW *tt;
	MIB_IFROW r;
	
	DWORD idx = if_nametoindex(iface); // 0 if error
	DWORD waddr = 0;
	inet_pton(AF_INET, iface, &waddr);

	if ( GetIpAddrTable(&u.t, &sz, FALSE) == 0 ) {
		for (DWORD i = 0; i < u.t.dwNumEntries; ++i) {
			tt = &u.t.table[i];
			if (!tt->dwIndex) continue;
			if (tt->dwIndex == idx || tt->dwAddr == waddr) {
				memset(&r, 0, sizeof(r));
				r.dwIndex = tt->dwIndex;
				GetIfEntry(&r);
				if (r.dwType == MIB_IF_TYPE_ETHERNET) {
					memcpy(addr, r.bPhysAddr, ETH_ALEN);
					return true;
				}
			}
		}
	}
	return false;
}

uint8_t *EtherDgramSocket_getSocketMACAddress(DgramSocket self, uint8_t *addr)
{
	if (self == NULL || addr == NULL) return NULL;
	if (self->s != INVALID_SOCKET) {
		char ifname[32];
		if ( EtherDgramSocket_getInterfaceMACAddress(
				(const char *)if_indextoname(self->ifidx, ifname),
				addr)
		) {
			return addr;
		}
	}
	return NULL;
}


bool DgramSocket_reset(DgramSocket self)
{
	if (self == NULL) return false;
	if (self->s != INVALID_SOCKET) {
		closesocket(self->s);
		uint16_t protocol = (self->protocol != -1)? (uint16_t)self->protocol : 0;
		self->s = socket(AF_INET, SOCK_DGRAM, protocol);
		if (self->s != INVALID_SOCKET) {
			return true;
		}
	}
	return false;
}

void DgramSocket_close(DgramSocket self) // hal internal use only
{
	if (self == NULL) return;
	if (self->s != INVALID_SOCKET) {
		closesocket(self->s);
		self->s = INVALID_SOCKET;
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

int DgramSocket_readFrom(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size)
{
	if (self == NULL || addr == NULL || buf == NULL) return -1;
	struct sockaddr_storage saddr;
	socklen_t addr_size;
	memset(&saddr, 0, sizeof(struct sockaddr_storage));
	switch (self->domain) {
		case AF_INET: {
			addr_size = sizeof(struct sockaddr_in);
		} break;
		case AF_UNIX: {
			addr_size = sizeof(struct sockaddr_in);
		} break;
		case AF_PACKET: {
			addr_size = sizeof(struct sockaddr_dl);
		} break;
		default: break;
	}
	int rc = recvfrom(self->s, buf, size, 0, (struct sockaddr *)&saddr, &addr_size);
	if (rc > 0) {
		switch (self->domain) {
			case AF_INET: {
				struct sockaddr_in *paddr = (struct sockaddr_in *)&saddr;
				inet_paddr(htonl(paddr->sin_addr.s_addr), addr->ip);
				addr->port = htons(paddr->sin_port);
			} break;
			case AF_UNIX: {
				struct sockaddr_in *paddr = (struct sockaddr_in *)&saddr;
				inet_paddr(htonl(paddr->sin_addr.s_addr), addr->ip);
				addr->port = htons(paddr->sin_port);
			} break;
			case AF_PACKET: {
				struct sockaddr_dl *paddr = (struct sockaddr_dl *)&saddr;
				memcpy(addr->mac, paddr->sdl_data, ETH_ALEN);
			} break;
			default: break;
		}
	}
	return rc;
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
			inet_pton(AF_INET, addr->ip, &paddr->sin_addr.s_addr);
			paddr->sin_family = AF_INET;
			paddr->sin_port = htons(addr->port);
		} break;
		case AF_UNIX: {
			struct sockaddr_in *paddr = (struct sockaddr_in *)&saddr;
			uint16_t port = Hal_generatePort(addr->address, HAL_LOCAL_SOCK_PORT_MIN, HAL_LOCAL_SOCK_PORT_MAX);
			inet_pton(AF_INET, HAL_LOCAL_SOCK_ADDR, &paddr->sin_addr.s_addr);
			paddr->sin_family = AF_INET;
			paddr->sin_port = htons(port);
		} break;
		case AF_PACKET: {
			const uint8_t *src = buf+6;
			struct sockaddr_dl *paddr = (struct sockaddr_dl *)&saddr;
			addr_size = sizeof(struct sockaddr_dl);
			memcpy(paddr->sdl_data, src, ETH_ALEN);
		} break;
		default: break;
	}
	return sendto(self->s, buf, size, 0, (const struct sockaddr *)&saddr, addr_size);
}

int DgramSocket_read(DgramSocket self, uint8_t *buf, int size)
{
	if (self == NULL || buf == NULL) return -1;
	union uDgramSocketAddress source;
	int rc = DgramSocket_readFrom(self, &source, buf, size);
	if (rc < 0) return rc;
	if (rc > 0) {
		switch (self->domain) {
			case AF_INET: {
				if (source.port == self->remote.port && strcmp(source.ip, self->remote.ip) == 0) {
					return rc;
				}
			} break;
			case AF_UNIX: {
				if (source.port == self->remote.port && strcmp(source.ip, self->remote.ip) == 0) {
					return rc;
				}
			} break;
			case AF_PACKET: {
				if (memcmp(source.mac, self->remote.mac, ETH_ALEN) == 0) {
					return rc;
				}
			} break;
			default: break;
		}
	}
	return 0;
}

int DgramSocket_write(DgramSocket self, const uint8_t *buf, int size)
{
	if (self == NULL || buf == NULL) return -1;
	return DgramSocket_writeTo(self, &self->remote, buf, size);
}

int DgramSocket_readAvailable(DgramSocket self)
{
	if (self == NULL) return -1;
	return getSocketAvailableToRead(self->s);
}

void DgramSocket_destroy(DgramSocket self)
{
	if (self == NULL) return;
	DgramSocket_close(self);
	HalShSys_deinit();
	free(self);
}

unidesc DgramSocket_getDescriptor(DgramSocket self)
{
	if (self) {
		unidesc ret;
		ret.u64 = (uint64_t)self->s;
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

static char *inet_paddr(uint32_t in, char *out)
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

static uint32_t getAnyIpFromInterface(ULONG idx)
{
	union {
		MIB_IPADDRTABLE t;
		char max[MIB_IPADDRTABLE_SZ_MAX];
	} u;
	ULONG sz = sizeof(u);
	MIB_IPADDRROW *tt;
	MIB_IFROW r;

	if ( GetIpAddrTable(&u.t, &sz, FALSE) == 0 ) {
		for (DWORD i = 0; i < u.t.dwNumEntries; ++i) {
			tt = &u.t.table[i];
			if (!tt->dwIndex) continue;
			if (tt->dwIndex == idx || tt->dwAddr == 0x0100007f) {
				memset(&r, 0, sizeof(r));
				r.dwIndex = tt->dwIndex;
				GetIfEntry(&r);
				if (r.dwType == MIB_IF_TYPE_ETHERNET) {
					return tt->dwAddr;
				}
			}
		}
	}
	return 0;
}


#endif // _WIN32 || _WIN64