
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

#define MIB_IPADDRTABLE_SZ_MAX (sizeof(DWORD) + sizeof(MIB_IPADDRROW)*128)
typedef union {
	MIB_IPADDRTABLE t;
	char max[MIB_IPADDRTABLE_SZ_MAX];
} MIB_IPADDRTABLE_U;

#define IP_INTERFACE_INFO_SZ_MAX (sizeof(DWORD) + sizeof(IP_ADAPTER_INDEX_MAP)*128)
typedef union {
	IP_INTERFACE_INFO t;
	char max[IP_INTERFACE_INFO_SZ_MAX];
} IP_INTERFACE_INFO_U;


bool NetwHlpr_getInterfaceMACAddress(const char *iface, uint8_t *addr)
{
	if (iface == NULL || addr == NULL) return false;
	int idx;

	if (NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL) == false)
		return false;

	DWORD icnt = 0;
	DWORD idxit = 0;
	DWORD inum;
	MIB_IF_ROW2 r;
	if ( GetNumberOfInterfaces(&inum) != 0 )
		return false;
	while (icnt < inum && idxit < 65000) {
		idxit++;
		memset(&r, 0, sizeof(r));
		r.InterfaceIndex = (NET_IFINDEX)idxit;
		if ( GetIfEntry2(&r) == 0 ) {
			icnt++;
			if (r.InterfaceIndex == (NET_IFINDEX)idx) {
				memcpy(addr, r.PhysicalAddress, ETH_ALEN);
				return true;
			}
		}
	}
	return false;
}

static char *getInterfaceNameByIdx(ULONG idx, char *buf)
{
	IP_INTERFACE_INFO_U u;
	ULONG sz = sizeof(u);
	MIB_IF_ROW2 r;
	if ( GetInterfaceInfo(&u.t, &sz) == 0 ) {
		for (LONG i = 0; i < u.t.NumAdapters; ++i) {
			IP_ADAPTER_INDEX_MAP *a = &u.t.Adapter[i];
			if (a->Index == idx) {
				memset(&r, 0, sizeof(r));
				r.InterfaceIndex = (NET_IFINDEX)a->Index;
				if ( GetIfEntry2(&r) == 0 ) {
					wchar_t szGuidW[40];
					StringFromGUID2(&r.InterfaceGuid, szGuidW, 40);
					WideCharToMultiByte(CP_ACP, 0, szGuidW, -1, buf, 40, NULL, NULL);
					return buf;
				}
			}
		}
	}
	return NULL;
}

static ULONG getInterfaceIdxByName(const char *name)
{
	IP_INTERFACE_INFO_U u;
	ULONG sz = sizeof(u);
	MIB_IF_ROW2 r;
	ULONG idx = if_nametoindex(name);
	if (idx > 0) {
		return idx;
	}
	if ( GetInterfaceInfo(&u.t, &sz) == 0 ) {
		for (LONG i = 0; i < u.t.NumAdapters; ++i) {
			IP_ADAPTER_INDEX_MAP *a = &u.t.Adapter[i];
			memset(&r, 0, sizeof(r));
			r.InterfaceIndex = (NET_IFINDEX)a->Index;
			if ( GetIfEntry2(&r) == 0 ) {
				char buf[40];
				wchar_t szGuidW[40];
				StringFromGUID2(&r.InterfaceGuid, szGuidW, 40);
				WideCharToMultiByte(CP_ACP, 0, szGuidW, -1, buf, 40, NULL, NULL);
				if (strcmp(buf, name) == 0) {
					return a->Index;
				}
			}
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
					Hal_ipv4BinToStr(tt->dwAddr, ipaddr);
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