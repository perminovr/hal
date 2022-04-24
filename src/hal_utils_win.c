
#if defined(_WIN32) || defined(_WIN64)

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <netioapi.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <windows.h>

#include "hal_utils.h"

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#define NPF_DRIVER_NPF 1
#if NPF_DRIVER_NPF
#define NPF_DRIVER_NAME							"NPF"
#define NPF_DEVICE_NAMES_PREFIX					NPF_DRIVER_NAME "_"
#else
#define NPF_DRIVER_NAME							"NPCAP"
#define NPF_DEVICE_NAMES_PREFIX					NPF_DRIVER_NAME "\\"
#endif
#define NPF_DRIVER_COMPLETE_DEVICE_PREFIX		"\\Device\\" NPF_DEVICE_NAMES_PREFIX

#define STORAGE_SZ_MAX (sizeof(DWORD) + sizeof(IP_ADAPTER_ADDRESSES_LH)*64)
typedef union {
	IP_ADAPTER_ADDRESSES t;
	char max[STORAGE_SZ_MAX];
} IP_ADAPTER_ADDRESSES_U;

#define MIB_IPADDRTABLE_SZ_MAX (sizeof(DWORD) + sizeof(MIB_IPADDRROW)*128)
typedef union {
	MIB_IPADDRTABLE t;
	char max[MIB_IPADDRTABLE_SZ_MAX];
} MIB_IPADDRTABLE_U;


bool NetwHlpr_getInterfaceMACAddress(const char *iface, uint8_t *addr)
{
	if (iface == NULL || addr == NULL) return false;

	MIB_IPADDRTABLE_U u;
	ULONG sz = sizeof(u);
	MIB_IPADDRROW *tt;
	MIB_IFROW r;
	int idx;

	if (NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL) == false) 
		return false;	

	if ( GetIpAddrTable(&u.t, &sz, FALSE) == 0 ) {
		for (DWORD i = 0; i < u.t.dwNumEntries; ++i) {
			tt = &u.t.table[i];
			if (!tt->dwIndex) continue;
			if (tt->dwIndex == (DWORD)idx) {
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

static char *getInterfaceNameByIdx(ULONG idx, char *buf)
{
	IP_ADAPTER_ADDRESSES_U u;
	ULONG sz = sizeof(u);
	// flags from PacketGetAdaptersNPF (packetWin7)
	GetAdaptersAddresses(AF_UNSPEC, 
			GAA_FLAG_INCLUDE_ALL_INTERFACES | // Get everything
			GAA_FLAG_SKIP_DNS_INFO | // Undocumented, reported to help avoid errors on Win10 1809
			// We don't use any of these features:
			GAA_FLAG_SKIP_DNS_SERVER |
			GAA_FLAG_SKIP_ANYCAST |
			GAA_FLAG_SKIP_MULTICAST |
			GAA_FLAG_SKIP_FRIENDLY_NAME, NULL, 
			&u.t, &sz);
	for (PIP_ADAPTER_ADDRESSES a = &u.t; a; a = a->Next) {
		if (a->IfIndex == idx) {
			sprintf(buf, "%s%s", NPF_DRIVER_COMPLETE_DEVICE_PREFIX, a->AdapterName);
			return buf;
		}
	}
	return NULL;
}

static ULONG getInterfaceIdxByName(const char *name)
{
	char buf[256];
	IP_ADAPTER_ADDRESSES_U u;
	ULONG sz = sizeof(u);
	ULONG idx = if_nametoindex(name);
	if (idx > 0) { 
		return idx; 
	}
	// flags from PacketGetAdaptersNPF (packetWin7)
	GetAdaptersAddresses(AF_UNSPEC, 
			GAA_FLAG_INCLUDE_ALL_INTERFACES | // Get everything
			GAA_FLAG_SKIP_DNS_INFO | // Undocumented, reported to help avoid errors on Win10 1809
			// We don't use any of these features:
			GAA_FLAG_SKIP_DNS_SERVER |
			GAA_FLAG_SKIP_ANYCAST |
			GAA_FLAG_SKIP_MULTICAST |
			GAA_FLAG_SKIP_FRIENDLY_NAME, NULL, 
			&u.t, &sz);
	for (PIP_ADAPTER_ADDRESSES a = &u.t; a; a = a->Next) {
		if (strcmp(a->AdapterName, name) == 0) {
			return a->IfIndex;
		}
		sprintf(buf, "%s%s", NPF_DRIVER_COMPLETE_DEVICE_PREFIX, a->AdapterName);
		if (strcmp(buf, name) == 0) {
			return a->IfIndex;
		}
	}
	return 0;
}

static void anyIpAddrFromIface(ULONG idx, char *ipaddr)
{
	MIB_IPADDRTABLE_U u;
	ULONG sz = sizeof(u);
	MIB_IPADDRROW *tt;
	MIB_IFROW r;
	if ( GetIpAddrTable(&u.t, &sz, FALSE) == 0 ) {
		for (DWORD i = 0; i < u.t.dwNumEntries; ++i) {
			tt = &u.t.table[i];
			if (!tt->dwIndex) continue;
			if (tt->dwIndex == idx) {
				memset(&r, 0, sizeof(r));
				r.dwIndex = tt->dwIndex;
				GetIfEntry(&r);
				if (r.dwType == MIB_IF_TYPE_ETHERNET) {
					Hal_ipv4BinToStr(htonl(tt->dwAddr), ipaddr);
					return;
				}
			}
		}
	}
}

static ULONG getInterfaceIdxByAddr(unsigned long iaddr)
{
	MIB_IPADDRTABLE_U u;
	ULONG sz = sizeof(u);
	MIB_IPADDRROW *tt;
	MIB_IFROW r;
	if ( GetIpAddrTable(&u.t, &sz, FALSE) == 0 ) {
		for (DWORD i = 0; i < u.t.dwNumEntries; ++i) {
			tt = &u.t.table[i];
			if (!tt->dwIndex) continue;
			if (tt->dwAddr == iaddr) {
				memset(&r, 0, sizeof(r));
				r.dwIndex = tt->dwIndex;
				GetIfEntry(&r);
				if (r.dwType == MIB_IF_TYPE_ETHERNET) {
					return tt->dwIndex;
				}
			}
		}
	}
	return 0;
}

bool NetwHlpr_interfaceInfo(const char *iface, int *num, char *name, char *name2, char *addr)
{
	if (!iface) return false;
	ULONG idx = 0;
	unsigned long iaddr;

	if (strlen(iface) < (sizeof("0.0.0.0")-1)) {
		if (sscanf(iface, "%d", &idx) == 1) {
			goto exit_found;
		}
	}
	idx = getInterfaceIdxByName(iface);
	if (idx > 0) {
		goto exit_found;
	}
	iaddr = inet_addr(iface);
	if (iaddr != INADDR_NONE) {
		idx = getInterfaceIdxByAddr(iaddr);
		goto exit_found;
	}

	return false;
	
exit_found:	
	if (num) { *num = idx; }
	if (name) { getInterfaceNameByIdx(idx, name); }
	if (name2) { if_indextoname(idx, name2); }
	if (addr) { anyIpAddrFromIface(idx, addr); }
	return true;
}

#endif // _WIN32 || _WIN64