
#if defined(_WIN32) || defined(_WIN64)

#include "hal_socket_dgram.h"
#include "hal_syshelper.h"
#include "hal_utils.h"

#ifdef HAL_PCAP_SUPPORTED

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

#define NPF_DRIVER_NPF 1
#if NPF_DRIVER_NPF
#define NPF_DRIVER_NAME							"NPF"
#define NPF_DEVICE_NAMES_PREFIX					NPF_DRIVER_NAME "_"
#else
#define NPF_DRIVER_NAME							"NPCAP"
#define NPF_DEVICE_NAMES_PREFIX					NPF_DRIVER_NAME "\\"
#endif
#define NPF_DRIVER_COMPLETE_DEVICE_PREFIX		"\\Device\\" NPF_DEVICE_NAMES_PREFIX


struct sEtherDgramSocket {
	int domain; // must be first
	int ifidx;
	uint16_t ethTypeFilter;
	pcap_t *s;
	ShDescDgram sdd;
	struct bpf_program bpfprg;
	uint8_t remotemac[6];
};
typedef struct sEtherDgramSocket * EtherDgramSocket;


static int hsfh_write_cb(void *user, uint8_t *buffer, int bufSize)
{
	EtherDgramSocket self = (EtherDgramSocket)user;
	if (!self->s) return 0;
	return pcap_sendpacket(self->s, buffer, bufSize);
}

static int hsfh_read_cb(void *user, uint8_t *buffer, int bufSize)
{
	EtherDgramSocket self = (EtherDgramSocket)user;
	if (!self->s) return 0;
	struct pcap_pkthdr *header;
	uint8_t *packetData;
	int rc = pcap_next_ex(self->s, &header, &packetData);
	if (rc == 1) {
		int sz = ((int)header->caplen < bufSize)? (int)header->caplen : bufSize;
		memcpy(buffer, packetData, sz);
		return sz;
	}
	return 0;
}

static bool EtherDgramSocket_bind(EtherDgramSocket self)
{
	pcap_t *s;
	HANDLE h;
	unidesc ud;
	ShDescDgram sdd;
	char buf[128];
	char ifname[64];
	char errbuf[PCAP_ERRBUF_SIZE] = {0};

	if (!self || self->s || self->sdd) return false;

	sprintf(buf, "%d", self->ifidx);
	if (NetwHlpr_interfaceInfo(buf, NULL, ifname, NULL, NULL) == false) return false;

	sprintf(buf, "%s%s", NPF_DRIVER_COMPLETE_DEVICE_PREFIX, ifname);
	s = pcap_open_live(buf, 65535, PCAP_OPENFLAG_PROMISCUOUS, 0, errbuf);
	if (!s) return false;

	sprintf(buf, "ether proto 0x%x", self->ethTypeFilter);
	if (pcap_compile(s, &self->bpfprg, buf, 1, PCAP_NETMASK_UNKNOWN) != 0) {
		goto pcap_exit;
	}
	if (pcap_setfilter(s, &self->bpfprg) != 0) {
		goto bpfprg_exit;
	}

	h = pcap_getevent(s);
	if (h == INVALID_HANDLE_VALUE) {
		goto bpfprg_exit;
	}
	ud.u64 = (uint64_t)h;
	sdd = ShDescDgram_create(ud, 1518);
	if (!sdd) {
		goto bpfprg_exit;
	}

	self->s = s;
	self->sdd = sdd;
	ShDescDgram_setSpecCallbacks(sdd, (void *)self, hsfh_read_cb, hsfh_write_cb);
	return true;
	
bpfprg_exit:
	pcap_freecode(&self->bpfprg);
pcap_exit:
	pcap_close(s);
	return false;
}

HAL_INTERNAL DgramSocket EtherDgramSocket_create0(const char *iface, uint16_t ethTypeFilter, int domain)
{
	if (iface == NULL) return NULL;
	HalShSys_init();

	EtherDgramSocket self;
	int idx;

	if (NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL) == false) return NULL;

	self = (EtherDgramSocket)calloc(1, sizeof(struct sEtherDgramSocket));
	if (self) {
		self->domain = domain;
		self->ifidx = idx;
		self->ethTypeFilter = ethTypeFilter;
		if (EtherDgramSocket_bind(self) == false) {
			goto self_exit;
		}
	}
	return (DgramSocket)self;

self_exit:
	free(self);
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
	ShDescDgram_destroy(self->sdd);
	pcap_freecode(&self->bpfprg);
	pcap_close(self->s);
	self->sdd = NULL;
	self->s = NULL;
}

HAL_INTERNAL bool EtherDgramSocket_reset(DgramSocket self0)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	EtherDgramSocket_close(self0);
	return EtherDgramSocket_bind(self);
}


HAL_INTERNAL void EtherDgramSocket_setRemote(DgramSocket self0, const DgramSocketAddress addr)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	if (self == NULL || addr == NULL) return;
	memcpy(&self->remotemac, addr->mac, ETH_ALEN);
}

HAL_INTERNAL void EtherDgramSocket_getRemote(DgramSocket self0, DgramSocketAddress addr)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	if (self == NULL || addr == NULL) return;
	memcpy(addr->mac, &self->remotemac, ETH_ALEN);
}


HAL_INTERNAL int EtherDgramSocket_peek(DgramSocket self0, DgramSocketAddress addr, uint8_t *buf, int size)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	int ret = ShDescDgram_peek(self->sdd, buf, sizeof(buf));
	if (ret > 0) {
		uint8_t *source = buf+ETH_ALEN;
		memcpy(addr->mac, source, ETH_ALEN);
	}
	return ret;
}

HAL_INTERNAL int EtherDgramSocket_readFrom(DgramSocket self0, DgramSocketAddress addr, uint8_t *buf, int size)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	int ret = ShDescDgram_read(self->sdd, buf, size);
	if (ret > 0) {
		uint8_t *source = buf+ETH_ALEN;
		memcpy(addr->mac, source, ETH_ALEN);
	}
	return ret;
}

HAL_INTERNAL int EtherDgramSocket_readAvailable(DgramSocket self0, bool fromRemote)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	uint8_t buf[1518];
	uint8_t *source;
	int ret, rc;

	while (1) {
		ret = ShDescDgram_readAvailable(self->sdd);
		if (ret <= 0) return ret;
		if (!fromRemote) return ret;
		//
		rc = ShDescDgram_peek(self->sdd, buf, sizeof(buf));
		if (rc > 0) {
			source = buf+ETH_ALEN;
			if (memcmp(source, self->remotemac, ETH_ALEN) == 0) {
				return ret;
			}
			ShDescDgram_read(self->sdd, buf, sizeof(buf)); // flush
		} else {
			return -1;
		}
	}
	return -1;
}

HAL_INTERNAL int EtherDgramSocket_read(DgramSocket self0, uint8_t *buf, int size)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	int rc = EtherDgramSocket_readAvailable(self0, true);
	if (rc <= 0) return rc;
	return ShDescDgram_read(self->sdd, buf, size);
}


HAL_INTERNAL int EtherDgramSocket_write(DgramSocket self0, const uint8_t *buf, int size)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	return ShDescDgram_write(self->sdd, buf, size);
}

HAL_INTERNAL int EtherDgramSocket_writeTo(DgramSocket self0, const DgramSocketAddress addr, const uint8_t *buf, int size)
{
	(void)addr;
	return EtherDgramSocket_write(self0, buf, size);
}


HAL_INTERNAL void EtherDgramSocket_destroy(DgramSocket self0)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	EtherDgramSocket_close(self0);
	HalShSys_deinit();
	free(self);
}

HAL_INTERNAL unidesc EtherDgramSocket_getDescriptor(DgramSocket self0)
{
	EtherDgramSocket self = (EtherDgramSocket)self0;
	return ShDescDgram_getDescriptor(self->sdd);
}


#else // HAL_PCAP_SUPPORTED


HAL_INTERNAL DgramSocket EtherDgramSocket_create0(const char *iface, uint16_t ethTypeFilter, int domain)
{ return NULL; }

HAL_INTERNAL void EtherDgramSocket_close(DgramSocket self)
{ return; }

HAL_INTERNAL int EtherDgramSocket_readFrom(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size)
{ return -1; }

HAL_INTERNAL int EtherDgramSocket_writeTo(DgramSocket self, const DgramSocketAddress addr, const uint8_t *buf, int size)
{ return -1; }

HAL_INTERNAL int EtherDgramSocket_read(DgramSocket self, uint8_t *buf, int size)
{ return -1; }

HAL_INTERNAL int EtherDgramSocket_write(DgramSocket self, const uint8_t *buf, int size)
{ return -1; }

HAL_INTERNAL int EtherDgramSocket_readAvailable(DgramSocket self, bool fromRemote)
{ return -1; }

HAL_INTERNAL void EtherDgramSocket_destroy(DgramSocket self)
{ return; }

HAL_INTERNAL unidesc EtherDgramSocket_getDescriptor(DgramSocket self)
{ return Hal_getInvalidUnidesc(); }

HAL_INTERNAL bool EtherDgramSocket_reset(DgramSocket self)
{ return false; }

HAL_INTERNAL void EtherDgramSocket_setRemote(DgramSocket self, const DgramSocketAddress addr)
{ return; }

HAL_INTERNAL void EtherDgramSocket_getRemote(DgramSocket self, DgramSocketAddress addr)
{ return; }

HAL_INTERNAL int EtherDgramSocket_peek(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size)
{ return -1; }


#endif // HAL_PCAP_SUPPORTED
#endif // _WIN32 || _WIN64