
#if defined(_WIN32) || defined(_WIN64)

#include "hal_socket_dgram.h"
#include "hal_syshelper.h"
#include "hal_utils.h"
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <netioapi.h>
#include <iphlpapi.h>
#include <windows.h>


typedef enum {
	ST_Inet,
	ST_Local,
	ST_Ether,
} SocketType_e;

struct sDgramSocket {
	int domain; // must be first (SocketType_e)
	SOCKET s;
	union uDgramSocketAddress remote;
	union uDgramSocketAddress localremote;
};


static bool prepareSocketAddress(const char *address, uint16_t port, struct sockaddr_in *sockaddr);

HAL_INTERNAL DgramSocket EtherDgramSocket_create0(const char *iface, uint16_t ethTypeFilter, int domain);
HAL_INTERNAL void EtherDgramSocket_close(DgramSocket self);
HAL_INTERNAL int EtherDgramSocket_readFrom(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size);
HAL_INTERNAL int EtherDgramSocket_writeTo(DgramSocket self, const DgramSocketAddress addr, const uint8_t *buf, int size);
HAL_INTERNAL int EtherDgramSocket_read(DgramSocket self, uint8_t *buf, int size);
HAL_INTERNAL int EtherDgramSocket_write(DgramSocket self, const uint8_t *buf, int size);
HAL_INTERNAL int EtherDgramSocket_readAvailable(DgramSocket self, bool fromRemote);
HAL_INTERNAL void EtherDgramSocket_destroy(DgramSocket self);
HAL_INTERNAL unidesc EtherDgramSocket_getDescriptor(DgramSocket self);
HAL_INTERNAL bool EtherDgramSocket_reset(DgramSocket self);
HAL_INTERNAL void EtherDgramSocket_setRemote(DgramSocket self, const DgramSocketAddress addr);
HAL_INTERNAL void EtherDgramSocket_getRemote(DgramSocket self, DgramSocketAddress addr);
HAL_INTERNAL int EtherDgramSocket_peek(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size);


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
	DgramSocket self;
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

	self = (DgramSocket)calloc(1, sizeof(struct sDgramSocket));
	if (self) {
		self->s = sock;
		self->domain = (int)ST_Inet;
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
	char localip[16];
	if (NetwHlpr_interfaceInfo(iface, NULL, NULL, NULL, localip) == false) return false;
	inet_pton(AF_INET, ip, &imreqn.imr_multiaddr.s_addr);
	inet_pton(AF_INET, localip, &imreqn.imr_interface.s_addr);
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

	uint16_t port = NetwHlpr_generatePort(address, HAL_LOCAL_SOCK_PORT_MIN, HAL_LOCAL_SOCK_PORT_MAX);
	DgramSocket self = UdpDgramSocket_createAndBind(HAL_LOCAL_SOCK_ADDR, port);

	if (self) {
		self->domain = (int)ST_Local;
	}

	return self;
}

void LocalDgramSocket_unlinkAddress(const char *address)
{
	DeleteFileA(address);
}


DgramSocket EtherDgramSocket_create(const char *iface, uint16_t ethTypeFilter)
{
	return EtherDgramSocket_create0(iface, ethTypeFilter, (int)ST_Ether);
}


static bool DgramSocket_reset0(DgramSocket self)
{
	if (self->s != INVALID_SOCKET) {
		closesocket(self->s);
		self->s = socket(AF_INET, SOCK_DGRAM, 0);
		if (self->s != INVALID_SOCKET) {
			setSocketNonBlocking(self->s);
			return true;
		}
	}
	return false;
}

bool DgramSocket_reset(DgramSocket self)
{
	if (self == NULL) return false;
	switch (self->domain) {
		case (int)ST_Inet: {
			return DgramSocket_reset0(self);
		} break;
		case (int)ST_Ether: {
			return EtherDgramSocket_reset(self);
		} break;
		default: break;
	}
	return false;
}

HAL_INTERNAL void DgramSocket_close(DgramSocket self)
{
	if (self == NULL) return;
	switch (self->domain) {
		case (int)ST_Local:
		case (int)ST_Inet: {
			if (self->s != INVALID_SOCKET) {
				closesocket(self->s);
				self->s = INVALID_SOCKET;
			}
		} break;
		case (int)ST_Ether: {
			EtherDgramSocket_close(self);
		} break;
		default: break;
	}
}

void DgramSocket_setRemote(DgramSocket self, const DgramSocketAddress addr)
{
	if (self == NULL || addr == NULL) return;
	switch (self->domain) {
		case (int)ST_Inet: {
			memcpy(&self->remote, addr, sizeof(union uDgramSocketAddress));
		} break; 
		case (int)ST_Local: {
			memcpy(&self->localremote, addr, sizeof(union uDgramSocketAddress));
			self->remote.port = NetwHlpr_generatePort(addr->address, HAL_LOCAL_SOCK_PORT_MIN, HAL_LOCAL_SOCK_PORT_MAX);
			strcpy(self->remote.ip, HAL_LOCAL_SOCK_ADDR);
		} break;
		case (int)ST_Ether: {
			EtherDgramSocket_setRemote(self, addr);
		} break;
		default: break;
	}
}

void DgramSocket_getRemote(DgramSocket self, DgramSocketAddress addr)
{
	if (self == NULL || addr == NULL) return;
	switch (self->domain) {
		case (int)ST_Inet: {
			memcpy(addr, &self->remote, sizeof(union uDgramSocketAddress));
		} break; 
		case (int)ST_Local: {
			memcpy(addr, &self->localremote, sizeof(union uDgramSocketAddress));
		} break;
		case (int)ST_Ether: {
			EtherDgramSocket_getRemote(self, addr);
		} break;
		default: break;
	}
}

static int socketReadFrom(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size, int flags)
{
	struct sockaddr_storage saddr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	memset(&saddr, 0, sizeof(struct sockaddr_storage));
	int rc = recvfrom(self->s, buf, size, flags, (struct sockaddr *)&saddr, &addr_size);
	if (rc > 0) {
		if (addr) {
			struct sockaddr_in *paddr = (struct sockaddr_in *)&saddr;
			Hal_ipv4BinToStr(htonl(paddr->sin_addr.s_addr), addr->ip);
			addr->port = htons(paddr->sin_port);
		}
	}
	return rc;
}

int DgramSocket_readFrom(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size)
{
	if (self == NULL || addr == NULL || buf == NULL) return -1;
	if (self->domain == (int)ST_Ether) {
		return EtherDgramSocket_readFrom(self, addr, buf, size);
	}
	return socketReadFrom(self, addr, buf, size, 0);
}

static int DgramSocket_writeTo0(DgramSocket self, const DgramSocketAddress addr, const uint8_t *buf, int size)
{
	struct sockaddr_storage saddr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	struct sockaddr_in *paddr = (struct sockaddr_in *)&saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_storage));
	const char *ip = addr->ip;
	uint16_t port = addr->port;
	if (self->domain == (int)ST_Local) {
		port = NetwHlpr_generatePort(addr->address, HAL_LOCAL_SOCK_PORT_MIN, HAL_LOCAL_SOCK_PORT_MAX);
		ip = HAL_LOCAL_SOCK_ADDR;
	}
	inet_pton(AF_INET, ip, &paddr->sin_addr.s_addr);
	paddr->sin_family = AF_INET;
	paddr->sin_port = htons(port);
	return sendto(self->s, buf, size, 0, (const struct sockaddr *)&saddr, addr_size);
}

int DgramSocket_writeTo(DgramSocket self, const DgramSocketAddress addr, const uint8_t *buf, int size)
{
	if (self == NULL || addr == NULL || buf == NULL) return -1;
	if (self->domain == (int)ST_Ether) {
		return EtherDgramSocket_writeTo(self, addr, buf, size);
	}
	return DgramSocket_writeTo0(self, addr, buf, size);
}

int DgramSocket_read(DgramSocket self, uint8_t *buf, int size)
{
	if (self == NULL || buf == NULL) return -1;
	if (self->domain == (int)ST_Ether) {
		return EtherDgramSocket_read(self, buf, size);
	}
	int rc = DgramSocket_readAvailable(self, true);
	if (rc <= 0) return rc;
	return socketReadFrom(self, NULL, buf, size, 0);
}

static int DgramSocket_write0(DgramSocket self, const uint8_t *buf, int size)
{
	DgramSocketAddress addr = &self->remote;
	struct sockaddr_storage saddr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	struct sockaddr_in *paddr = (struct sockaddr_in *)&saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_storage));
	inet_pton(AF_INET, addr->ip, &paddr->sin_addr.s_addr);
	paddr->sin_family = AF_INET;
	paddr->sin_port = htons(addr->port);
	return sendto(self->s, buf, size, 0, (const struct sockaddr *)&saddr, addr_size);
}

int DgramSocket_write(DgramSocket self, const uint8_t *buf, int size)
{
	if (self == NULL || buf == NULL) return -1;
	if (self->domain == (int)ST_Ether) {
		return EtherDgramSocket_write(self, buf, size);
	}
	return DgramSocket_write0(self, buf, size);
}

static int DgramSocket_readAvailable0(DgramSocket self, bool fromRemote)
{
	uint8_t buf[65535]; // WSAEMSGSIZE
	union uDgramSocketAddress source;
	int ret, rc;
	while (1) {
		ret = getSocketAvailableToRead(self->s);
		if (ret <= 0) return ret;
		if (!fromRemote) return ret;
		//
		rc = socketReadFrom(self, &source, buf, sizeof(buf), MSG_PEEK);
		if (rc > 0) {
			if (source.port == self->remote.port && strcmp(source.ip, self->remote.ip) == 0) {
				return ret;
			}
			socketReadFrom(self, &source, buf, sizeof(buf), 0); // flush
		} else {
			return -1;
		}
	}
}

int DgramSocket_readAvailable(DgramSocket self, bool fromRemote)
{
	if (self == NULL) return -1;
	if (self->domain == (int)ST_Ether) {
		return EtherDgramSocket_readAvailable(self, fromRemote);
	}
	return DgramSocket_readAvailable0(self, fromRemote);
}

int DgramSocket_peek(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size)
{
	if (self == NULL || addr == NULL || buf == NULL) return -1;
	if (self->domain == (int)ST_Ether) {
		return EtherDgramSocket_peek(self, addr, buf, size);
	}
	return socketReadFrom(self, addr, buf, size, MSG_PEEK);
}

void DgramSocket_destroy(DgramSocket self)
{
	if (self == NULL) return;
	if (self->domain == (int)ST_Ether) {
		EtherDgramSocket_destroy(self);
		return;
	}
	DgramSocket_close(self);
	HalShSys_deinit();
	free(self);
}

unidesc DgramSocket_getDescriptor(DgramSocket self)
{
	if (self) {
		if (self->domain == (int)ST_Ether) {
			return EtherDgramSocket_getDescriptor(self);
		}
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


#endif // _WIN32 || _WIN64