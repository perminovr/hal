
#if defined(_WIN32) || defined(_WIN64)

#include "hal_netsys.h"
#include "hal_utils.h"

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <netioapi.h>
#include <iphlpapi.h>
#include <windows.h>


struct sNetsys {
	int unused;
};

#define IP_ADAPTER_INFO_SZ_MAX (sizeof(DWORD) + sizeof(IP_ADAPTER_INFO)*64)
typedef union {
	IP_ADAPTER_INFO t;
	char max[IP_ADAPTER_INFO_SZ_MAX];
} IP_ADAPTER_INFO_U;

#define MIB_IPFORWARDTABLE_SZ_MAX (sizeof(DWORD) + sizeof(MIB_IPFORWARDTABLE)*128)
typedef union {
	MIB_IPFORWARDTABLE t;
	char max[MIB_IPFORWARDTABLE_SZ_MAX];
} MIB_IPFORWARDTABLE_U;


bool Netsys_addIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority)
{
	if (!self) return false;
	if (!iface) return false;
	if (!destIP && !gwIP) return false;
	if (destIP && !destMask) return false;
	int idx;
	if (!NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL)) return false;

	MIB_IPFORWARDROW ifr;
	memset(&ifr, 0, sizeof(MIB_IPFORWARDROW));
	ifr.dwForwardIfIndex = (IF_INDEX)idx;
	ifr.dwForwardNextHop = (DWORD)Hal_ipv4StrToBin(gwIP);
	ifr.dwForwardDest = (DWORD)Hal_ipv4StrToBin(destIP);
	ifr.dwForwardMask = (DWORD)Hal_ipv4StrToBin(destMask);
	ifr.dwForwardMetric1 = (DWORD)priority;
	ifr.dwForwardProto = MIB_IPPROTO_NETMGMT;
	DWORD res = CreateIpForwardEntry(&ifr);
	return res == NO_ERROR;
}

bool Netsys_delIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority)
{
	if (!self) return false;
	if (!iface) return false;
	if (!destIP && !gwIP) return false;
	if (destIP && !destMask) return false;
	int idx;
	if (!NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL)) return false;

	MIB_IPFORWARDROW ifr;
	memset(&ifr, 0, sizeof(MIB_IPFORWARDROW));
	ifr.dwForwardIfIndex = (IF_INDEX)idx;
	ifr.dwForwardNextHop = (DWORD)Hal_ipv4StrToBin(gwIP);
	ifr.dwForwardDest = (DWORD)Hal_ipv4StrToBin(destIP);
	ifr.dwForwardMask = (DWORD)Hal_ipv4StrToBin(destMask);
	ifr.dwForwardMetric1 = (DWORD)priority;
	ifr.dwForwardProto = MIB_IPPROTO_NETMGMT;
	DWORD res = DeleteIpForwardEntry(&ifr);
	return res == NO_ERROR;
}


bool Netsys_addIpAddr(Netsys self, const char *iface,
		const char *ipaddr, const char *ipmask,
		const char *labelstr, int metric)
{
	if (!self) return false;
	if (!iface) return false;
	if (!ipaddr || !ipmask) return false;
	int idx;
	if (!NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL)) return false;
	ULONG NTEContext, NTEInstance;
	DWORD res = AddIPAddress(
		(IPAddr)Hal_ipv4StrToBin(ipaddr), (IPAddr)Hal_ipv4StrToBin(ipmask),
		(DWORD)idx, &NTEContext, &NTEInstance);
	return res == NO_ERROR;
}

bool Netsys_delIpAddr(Netsys self, const char *iface,
		const char *ipaddr, const char *ipmask)
{
	if (!self) return false;
	if (!iface) return false;
	if (!ipaddr || !ipmask) return false;
	int idx;
	if (!NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL)) return false;

	IP_ADAPTER_INFO_U u;
	ULONG sz = sizeof(u);
	if ( GetAdaptersInfo(&u.t, &sz) == 0 ) {
		for (PIP_ADAPTER_INFO i = &u.t; i; i = i->Next) {
			if (i->Index == (DWORD)idx) {
				for (PIP_ADDR_STRING j = &i->IpAddressList; j; j = j->Next) {
					if (strcmp(ipaddr, j->IpAddress.String) == 0) {
						if (strcmp(ipmask, j->IpMask.String) == 0) {
							DWORD res = DeleteIPAddress(j->Context);
							return res == NO_ERROR;
						}
					}
				}
				break;
			}
		}
	}
	return false;
}


bool Netsys_controlLink(Netsys self, const char *iface, bool up, bool promisc)
{
	if (!self) return false;
	if (!iface) return false;
	int idx;
	if (!NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL)) return false;
	MIB_IFROW ifr;
	memset(&ifr, 0, sizeof(MIB_IFROW));
	ifr.dwIndex = (DWORD)idx;
	ifr.dwAdminStatus = ((up)? MIB_IF_ADMIN_STATUS_UP : MIB_IF_ADMIN_STATUS_DOWN);
	DWORD res = SetIfEntry(&ifr); // todo does not work
	return res == NO_ERROR;
}


bool Netsys_findAllRoutes(Netsys self, const char *iface, NetSysRoute_t *out, int *size)
{
	if (!self) return false;
	if (!iface) return false;
	if (!out || !size || *size <= 0) return false;
	int idx;
	if (!NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL)) return false;

	MIB_IPFORWARDTABLE_U u;
	ULONG sz = sizeof(u);
	MIB_IPFORWARDROW *tt;
	int cnt = 0;
	bool found = false;
	char buf[16];
	if ( GetIpForwardTable(&u.t, &sz, FALSE) == 0) {
		for (DWORD i = 0; i < u.t.dwNumEntries; ++i) {
			tt = &u.t.table[i];
			if (tt->dwForwardIfIndex == (IF_INDEX)idx) {
				NetSysRoute_t *d = &out[cnt++];
				found = true;
				d->gwip = (uint32_t)tt->dwForwardNextHop;
				d->dstip = (uint32_t)tt->dwForwardDest;
				d->dstpfx = NetwHlpr_maskToPrefix(Hal_ipv4BinToStr((uint32_t)tt->dwForwardMask, buf)); // todo test
				d->prio = (uint32_t)tt->dwForwardMetric1;
				d->srcip = 0;
				if (cnt == *size) return true;
			}
		}
	}
	if (found) { *size = cnt; }
	return found;
}


bool Netsys_findAllIpAddrs(Netsys self, const char *iface, NetSysIpAddr_t *out, int *size)
{
	if (!self) return false;
	if (!iface) return false;
	if (!out || !size || *size <= 0) return false;
	int idx;
	if (!NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL)) return false;

	IP_ADAPTER_INFO_U u;
	ULONG sz = sizeof(u);
	int cnt = 0;
	bool found = false;
	if ( GetAdaptersInfo(&u.t, &sz) == 0 ) {
		for (PIP_ADAPTER_INFO i = &u.t; i; i = i->Next) {
			if (i->Index == (DWORD)idx) {
				for (PIP_ADDR_STRING j = &i->IpAddressList; j; j = j->Next) {
					NetSysIpAddr_t *d = &out[cnt++];
					found = true;
					d->ip = Hal_ipv4StrToBin(j->IpAddress.String);
					d->pfx = NetwHlpr_maskToPrefix(j->IpMask.String);
					if (cnt == *size) return true;
				}
				break;
			}
		}
	}
	if (found) { *size = cnt; }
	return found;
}


bool Netsys_findAllIfaces(Netsys self, NetSysIfaceType_e filter, NetSysIface_t *out, int *size)
{
	if (!self) return false;
	if (!out || !size || *size <= 0) return false;

	DWORD icnt = 0;
	DWORD idxit = 0;
	DWORD inum;
	MIB_IF_ROW2 r;
	int cnt = 0;
	bool found = false;
	uint32_t ufilter = (uint32_t)filter;
	if ( GetNumberOfInterfaces(&inum) != 0 )
		return false;
	while (icnt < inum && idxit < 65000) {
		idxit++;
		memset(&r, 0, sizeof(r));
		r.InterfaceIndex = (NET_IFINDEX)idxit;
		if ( GetIfEntry2(&r) == 0 ) {
			icnt++;
			if (
				r.InterfaceAndOperStatusFlags.HardwareInterface ||
				r.MediaType == NdisMediumLoopback
			) {
				NetSysIface_t *d = &out[cnt];
				#define addIfaceToOut(t) {\
					d->type = t;\
					sprintf(d->idx, "%u", r.InterfaceIndex);\
					found = true;\
					cnt++;\
					if (cnt == *size) return true;\
					continue;\
				}
				if (ufilter == (uint32_t)NSIT_Any) {
					if (r.MediaType == NdisMedium802_3)
						addIfaceToOut(NSIT_Ether);
					if (r.MediaType == NdisMediumNative802_11)
						addIfaceToOut(NSIT_Wifi);
					if (r.MediaType == NdisMediumLoopback)
						addIfaceToOut(NSIT_Loopback);
					addIfaceToOut(NSIT_Other);
				}
				if (r.MediaType == NdisMedium802_3 && (ufilter & (uint32_t)NSIT_Ether))
					addIfaceToOut(NSIT_Ether);
				if (r.MediaType == NdisMediumNative802_11 && (ufilter & (uint32_t)NSIT_Wifi))
					addIfaceToOut(NSIT_Wifi);
				if (r.MediaType == NdisMediumLoopback && (ufilter & (uint32_t)NSIT_Loopback))
					addIfaceToOut(NSIT_Loopback);
				if ((ufilter & (uint32_t)NSIT_Other))
					addIfaceToOut(NSIT_Other);
			}
		}
	}
	if (found) { *size = cnt; }
	return found;
}


bool Netsys_cleanupIface(Netsys self, const char *iface)
{
	if (!self) return false;
	if (!iface) return false;
	int idx;
	if (!NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL)) return false;

	{ // del routes
		MIB_IPFORWARDTABLE_U u;
		ULONG sz = sizeof(u);
		MIB_IPFORWARDROW *tt;
		if ( GetIpForwardTable(&u.t, &sz, FALSE) == 0) {
			for (DWORD i = 0; i < u.t.dwNumEntries; ++i) {
				tt = &u.t.table[i];
				if (tt->dwForwardIfIndex == (IF_INDEX)idx) {
					DeleteIpForwardEntry(tt);
				}
			}
		}
	}

	{ // del ip addrs
		IP_ADAPTER_INFO_U u;
		ULONG sz = sizeof(u);
		if ( GetAdaptersInfo(&u.t, &sz) == 0 ) {
			for (PIP_ADAPTER_INFO i = &u.t; i; i = i->Next) {
				if (i->Index == (DWORD)idx) {
					for (PIP_ADDR_STRING j = &i->IpAddressList; j; j = j->Next) {
						DeleteIPAddress(j->Context);
					}
					break;
				}
			}
		}
	}

	return true;
}


Netsys Netsys_create(void)
{
	Netsys self;

	self = (Netsys)calloc(1, sizeof(struct sNetsys));
	if (self) {
		return self;
	}

	return NULL;
}

void Netsys_destroy(Netsys self)
{
	if (!self) return;
	free(self);
}

#endif // defined(_WIN32) || defined(_WIN64)