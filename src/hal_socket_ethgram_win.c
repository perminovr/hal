
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

#define HAVE_REMOTE
#define WPCAP
#include "pcap.h"

#define ETH_ALEN	6
#define MIB_IPADDRTABLE_SZ_MAX (sizeof(DWORD) + sizeof(MIB_IPADDRROW)*128)


struct sEtherDgramSocket {
	int domain; // must be first
	int ifidx;
	pcap_t *s;
	ShDescDgram sdd;
};
typedef struct sEtherDgramSocket * EtherDgramSocket;


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


HAL_INTERNAL DgramSocket EtherDgramSocket_create0(const char *iface, uint16_t ethTypeFilter, int domain)
{
	if (iface == NULL) return NULL;
	HalShSys_init();

	EtherDgramSocket self;
	pcap_t *s;
	HANDLE h;
	unidesc ud;
	ShDescDgram sdd;
	int idx;
	char ifname[128];
	char errbuf[PCAP_ERRBUF_SIZE] = {0};

	if (NetwHlpr_interfaceInfo(iface, &idx, ifname, NULL, NULL) == false) return NULL;
	s = pcap_open_live(ifname, 65535, PCAP_OPENFLAG_PROMISCUOUS, 0, errbuf);
	if (!s) return NULL;

	// todo ethTypeFilter
	// ether proto protocol
	// pcap_compile
	// pcap_setfilter

	h = pcap_getevent(s);
	if (h == INVALID_HANDLE_VALUE) {
		goto pcap_exit;
	}
	ud.u64 = (uint64_t)h;
	sdd = ShDescDgram_create(ud, 1518);
	if (!sdd) {
		goto pcap_exit;
	}

	self = (EtherDgramSocket)calloc(1, sizeof(struct sEtherDgramSocket));
	if (self) {
		self->domain = domain;
		self->ifidx = idx;
		self->s = s;
		self->sdd = sdd;
	}
	return (DgramSocket)self;

	ShDescDgram_destroy(sdd);
pcap_exit:
	pcap_close(s);
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

uint8_t *EtherDgramSocket_getMACAddress(DgramSocket self0, uint8_t *addr)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	if (self == NULL || addr == NULL) return NULL;
	char ifidx[32];
	sprintf(ifidx, "%d", self->ifidx);
	if ( NetwHlpr_getInterfaceMACAddress(ifidx, addr) ) {
		return addr;
	}
	return NULL;
}


HAL_INTERNAL void EtherDgramSocket_close(DgramSocket self0)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	if (self == NULL) return;
	ShDescDgram_destroy(self->sdd);
	pcap_close(self->s);
}

static int socketReadFrom(EtherDgramSocket self0, DgramSocketAddress addr, uint8_t *buf, int size, int flags)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	return -1;
}

HAL_INTERNAL int EtherDgramSocket_readFrom(DgramSocket self0, DgramSocketAddress addr, uint8_t *buf, int size)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	if (self == NULL || addr == NULL || buf == NULL) return -1;
	return socketReadFrom(self, addr, buf, size, 0);
}

HAL_INTERNAL int EtherDgramSocket_writeTo(DgramSocket self0, const DgramSocketAddress addr, const uint8_t *buf, int size)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	return -1;
}

HAL_INTERNAL int EtherDgramSocket_readAvailable(DgramSocket self, bool fromRemote, uint8_t *buf);
HAL_INTERNAL int EtherDgramSocket_read(DgramSocket self0, uint8_t *buf, int size)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	if (self == NULL || buf == NULL) return -1;
	int rc = EtherDgramSocket_readAvailable(self0, true, NULL);
	if (rc <= 0) return rc;
	return socketReadFrom(self, NULL, buf, size, 0);
}

HAL_INTERNAL int EtherDgramSocket_write(DgramSocket self0, const uint8_t *buf, int size)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	return -1;
}

HAL_INTERNAL int EtherDgramSocket_readAvailable(DgramSocket self0, bool fromRemote, uint8_t *buf)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	return -1;
}

HAL_INTERNAL void EtherDgramSocket_destroy(DgramSocket self0)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	if (self == NULL) return;
	HalShSys_deinit();
	free(self);
}

HAL_INTERNAL unidesc EtherDgramSocket_getDescriptor(DgramSocket self0)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	if (self) {
		if (self->sdd) {
			return ShDescDgram_getDescriptor(self->sdd);
		}
	}
	return Hal_getInvalidUnidesc();
}


#endif // _WIN32 || _WIN64