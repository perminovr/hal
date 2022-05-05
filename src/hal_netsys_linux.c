#ifdef __linux__

#include "hal_netsys.h"
#include "hal_utils.h"

#ifdef HAL_LIBMNL_SUPPORTED

#include <asm/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if_arp.h>

#include <libmnl/libmnl.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <errno.h>
#include <fcntl.h>

#include <time.h>
#include <unistd.h>


#define CLEANUP_ROUTE_MAX	128
#define CLEANUP_IPADDR_MAX	128


struct sNetsys {
	struct mnl_socket *nl;
	char *buf;
	unsigned int seq;
	unsigned int portid;
	struct {
		int idx;
		struct {
			NetSysRoute_t *self;
			int it;
			int max;
		} route;
		struct {
			NetSysIpAddr_t *self;
			int it;
			int max;
		} ipaddr;
		struct {
			NetSysIface_t *self;
			int it;
			int max;
			uint32_t ufilter;
		} link;
	} searchmem;
};


static inline void setSocketNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


static int route_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = (const struct nlattr **)data;
	int type = mnl_attr_get_type(attr);
	if (mnl_attr_type_valid(attr, RTA_MAX) < 0)
		return MNL_CB_OK; /* skip unsupported attribute in user-space */
	switch(type) {
		case RTA_TABLE:
		case RTA_DST:
		case RTA_SRC:
		case RTA_OIF:
		case RTA_FLOW:
		case RTA_PREFSRC:
		case RTA_GATEWAY:
		case RTA_PRIORITY:
			if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
				return MNL_CB_ERROR;
			}
			break;
		case RTA_METRICS:
			if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {
				return MNL_CB_ERROR;
			}
			break;
		default: break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static int mnltalk_route_cb(const struct nlmsghdr *nlh, void *data)
{
	Netsys self = (Netsys)data;
	struct nlattr *tb[RTA_MAX+1] = {};
	struct rtmsg *rtm = (struct rtmsg *)mnl_nlmsg_get_payload(nlh);
	struct in_addr *inaddr;
	NetSysRoute_t *route;

	if (rtm->rtm_family == AF_INET) {
		mnl_attr_parse(nlh, sizeof(*rtm), route_attr_cb, tb);

		if (!tb[RTA_TABLE] || !tb[RTA_OIF])
			return MNL_CB_OK;

		if (mnl_attr_get_u32(tb[RTA_TABLE]) != RT_TABLE_MAIN)
			return MNL_CB_OK;

		if (mnl_attr_get_u32(tb[RTA_OIF]) != (uint32_t)self->searchmem.idx)
			return MNL_CB_OK;

		route = &self->searchmem.route.self[self->searchmem.route.it++];
		if (self->searchmem.route.it >= self->searchmem.route.max) return MNL_CB_OK;

		memset(route, 0, sizeof(NetSysRoute_t));
		if (tb[RTA_DST]) {
			inaddr = (struct in_addr *)mnl_attr_get_payload(tb[RTA_DST]);
			route->dstip = htonl(inaddr->s_addr);
			route->dstpfx = rtm->rtm_dst_len;
		}
		if (tb[RTA_PREFSRC]) {
			inaddr = (struct in_addr *)mnl_attr_get_payload(tb[RTA_PREFSRC]);
			route->srcip = htonl(inaddr->s_addr);
		}
		if (tb[RTA_GATEWAY]) {
			inaddr = (struct in_addr *)mnl_attr_get_payload(tb[RTA_GATEWAY]);
			route->gwip = htonl(inaddr->s_addr);
		}
		if (tb[RTA_PRIORITY]) {
			route->prio = mnl_attr_get_u32(tb[RTA_PRIORITY]);
		}
	}

	return MNL_CB_OK;
}


static int ipaddr_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = (const struct nlattr **)data;
	int type = mnl_attr_get_type(attr);
	if (mnl_attr_type_valid(attr, IFA_MAX) < 0)
		return MNL_CB_OK; /* skip unsupported attribute in user-space */
	switch(type) {
		case IFA_ADDRESS:
			if (mnl_attr_validate(attr, MNL_TYPE_BINARY) < 0) {
				return MNL_CB_ERROR;
			}
			break;
		default: break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static int mnltalk_ipaddr_cb(const struct nlmsghdr *nlh, void *data)
{
	Netsys self = (Netsys)data;
	struct nlattr *tb[IFA_MAX + 1] = {};
	struct ifaddrmsg *ifa = (struct ifaddrmsg *)mnl_nlmsg_get_payload(nlh);
	struct in_addr *inaddr;
	NetSysIpAddr_t *ipaddr;

	if (ifa->ifa_family == AF_INET && (int)ifa->ifa_index == self->searchmem.idx) {
		mnl_attr_parse(nlh, sizeof(*ifa), ipaddr_attr_cb, tb);

		if (!tb[IFA_ADDRESS])
			return MNL_CB_OK;

		ipaddr = &self->searchmem.ipaddr.self[self->searchmem.ipaddr.it++];
		if (self->searchmem.ipaddr.it >= self->searchmem.ipaddr.max) return MNL_CB_OK;
		{
			inaddr = (struct in_addr *)mnl_attr_get_payload(tb[IFA_ADDRESS]);
			ipaddr->ip = htonl(inaddr->s_addr);
			ipaddr->pfx = ifa->ifa_prefixlen;
		}
	}

	return MNL_CB_OK;
}

static int mnltalk_link_cb(const struct nlmsghdr *nlh, void *data)
{
	Netsys self = (Netsys)data;
	struct ifinfomsg *ifm = (struct ifinfomsg *)mnl_nlmsg_get_payload(nlh);
	NetSysIface_t *link;

	link = &self->searchmem.link.self[self->searchmem.link.it++];
	if (self->searchmem.link.it >= self->searchmem.link.max) return MNL_CB_OK;

	#define addIfaceToOut(t) {\
		link->type = t;\
		sprintf(link->idx, "%d", ifm->ifi_index);\
		return MNL_CB_OK;\
	}
	if (self->searchmem.link.ufilter == (uint32_t)NSIT_Any) {
		if (ifm->ifi_type == ARPHRD_ETHER || ifm->ifi_type == ARPHRD_IEEE802)
			addIfaceToOut(NSIT_Ether);
		if (ifm->ifi_type == ARPHRD_IEEE80211)
			addIfaceToOut(NSIT_Wifi);
		if (ifm->ifi_type == ARPHRD_LOOPBACK)
			addIfaceToOut(NSIT_Loopback);
		addIfaceToOut(NSIT_Other);
	}
	if ((ifm->ifi_type == ARPHRD_ETHER || ifm->ifi_type == ARPHRD_IEEE802) && (self->searchmem.link.ufilter & (uint32_t)NSIT_Ether))
		addIfaceToOut(NSIT_Ether);
	if (ifm->ifi_type == ARPHRD_IEEE80211 && (self->searchmem.link.ufilter & (uint32_t)NSIT_Wifi))
		addIfaceToOut(NSIT_Wifi);
	if (ifm->ifi_type == ARPHRD_LOOPBACK && (self->searchmem.link.ufilter & (uint32_t)NSIT_Loopback))
		addIfaceToOut(NSIT_Loopback);
	if ((self->searchmem.link.ufilter & (uint32_t)NSIT_Other))
		addIfaceToOut(NSIT_Other);

	return MNL_CB_OK;
}


static bool mnltalk(Netsys self, struct nlmsghdr *nlh, mnl_cb_t cb)
{
	int rc, cnt = 0;
	unsigned int seq;
	seq = nlh->nlmsg_seq = self->seq++;
	if (mnl_socket_sendto(self->nl, nlh, nlh->nlmsg_len) < 0) {
		return false;
	}
	do {
		rc = mnl_socket_recvfrom(self->nl, self->buf, MNL_SOCKET_BUFFER_SIZE);
		if (rc <= 0) {
			if (errno != EAGAIN) return false;
			break;
		}
		cnt++;
		rc = mnl_cb_run(self->buf, rc, seq, self->portid, cb, (void*)self);
		if (rc <= MNL_CB_ERROR)
			return false;
	} while (rc > 0);
	return (cnt > 0);
}


static bool Netsys_doIpRoute(Netsys self,
		int action,
		const char *iface,
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

	struct nlmsghdr *nlh;
	struct rtmsg *rtm;
	uint32_t binaddr;
	nlh = mnl_nlmsg_put_header(self->buf);
	nlh->nlmsg_type	= action;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE | NLM_F_ACK;
	rtm = (struct rtmsg *)mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtmsg));
	rtm->rtm_family = AF_INET;
	rtm->rtm_src_len = 0;
	rtm->rtm_tos = 0;
	rtm->rtm_protocol = RTPROT_STATIC;
	rtm->rtm_table = RT_TABLE_MAIN;
	rtm->rtm_type = RTN_UNICAST;
	rtm->rtm_flags = 0;
	if (destIP) {
		rtm->rtm_dst_len = NetwHlpr_maskToPrefix(destMask);
		binaddr = Hal_ipv4StrToBin(destIP);
		mnl_attr_put_u32(nlh, RTA_DST, binaddr);
	}
	if (srcIP) {
		binaddr = Hal_ipv4StrToBin(srcIP);
		mnl_attr_put_u32(nlh, RTA_PREFSRC, binaddr);
	}
	mnl_attr_put_u32(nlh, RTA_OIF, idx);
	if (gwIP) {
		binaddr = Hal_ipv4StrToBin(gwIP);
		mnl_attr_put_u32(nlh, RTA_GATEWAY, binaddr);
		rtm->rtm_scope = RT_SCOPE_UNIVERSE;
	} else {
		rtm->rtm_scope = RT_SCOPE_LINK;
	}
	mnl_attr_put_u32(nlh, RTA_PRIORITY, priority);

	return mnltalk(self, nlh, NULL);
}

bool Netsys_addIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority)
{
	return Netsys_doIpRoute(self, RTM_NEWROUTE, iface, srcIP, destIP, destMask, gwIP, priority);
}

bool Netsys_delIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority)
{
	return Netsys_doIpRoute(self, RTM_DELROUTE, iface, srcIP, destIP, destMask, gwIP, priority);
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

	struct nlmsghdr *nlh;
	struct ifaddrmsg *ifm;
	uint32_t binaddr;
	nlh = mnl_nlmsg_put_header(self->buf);
	nlh->nlmsg_type	= RTM_NEWADDR;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE | NLM_F_ACK;
	ifm = (struct ifaddrmsg *)mnl_nlmsg_put_extra_header(nlh, sizeof(struct ifaddrmsg));
	ifm->ifa_family = AF_INET;
	ifm->ifa_prefixlen = NetwHlpr_maskToPrefix(ipmask);
	ifm->ifa_flags = IFA_F_PERMANENT;
	ifm->ifa_scope = RT_SCOPE_UNIVERSE;
	ifm->ifa_index = idx;
	binaddr = Hal_ipv4StrToBin(ipaddr);
	mnl_attr_put_u32(nlh, IFA_LOCAL, binaddr);
	mnl_attr_put_u32(nlh, IFA_ADDRESS, binaddr);
	if (labelstr) { mnl_attr_put_str(nlh, IFA_LABEL, labelstr); }
	mnl_attr_put_u32(nlh, IFA_RT_PRIORITY, metric);
	return mnltalk(self, nlh, NULL);
}

bool Netsys_delIpAddr(Netsys self, const char *iface,
		const char *ipaddr, const char *ipmask)
{
	if (!self) return false;
	if (!iface) return false;
	if (!ipaddr || !ipmask) return false;
	int idx;
	if (!NetwHlpr_interfaceInfo(iface, &idx, NULL, NULL, NULL)) return false;

	struct nlmsghdr *nlh;
	struct ifaddrmsg *ifm;
	uint32_t binaddr;
	nlh = mnl_nlmsg_put_header(self->buf);
	nlh->nlmsg_type	= RTM_DELADDR;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE | NLM_F_ACK;
	ifm = (struct ifaddrmsg *)mnl_nlmsg_put_extra_header(nlh, sizeof(struct ifaddrmsg));
	ifm->ifa_family = AF_INET;
	ifm->ifa_prefixlen = NetwHlpr_maskToPrefix(ipmask);
	ifm->ifa_flags = IFA_F_PERMANENT;
	ifm->ifa_scope = RT_SCOPE_UNIVERSE;
	ifm->ifa_index = idx;
	binaddr = Hal_ipv4StrToBin(ipaddr);
	mnl_attr_put_u32(nlh, IFA_LOCAL, binaddr);
	mnl_attr_put_u32(nlh, IFA_ADDRESS, binaddr);
	return mnltalk(self, nlh, NULL);
}


bool Netsys_controlLink(Netsys self, const char *iface, bool up, bool promisc)
{
	if (!self) return false;
	if (!iface) return false;
	char sysiface[16];
	if (!NetwHlpr_interfaceInfo(iface, NULL, sysiface, NULL, NULL)) return false;

	struct nlmsghdr *nlh;
	struct ifinfomsg *ifm;
	nlh = mnl_nlmsg_put_header(self->buf);
	nlh->nlmsg_type	= RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	ifm = (struct ifinfomsg *)mnl_nlmsg_put_extra_header(nlh, sizeof(struct ifinfomsg));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change |= IFF_UP|IFF_PROMISC;
	if (up) {
		ifm->ifi_flags |= IFF_UP;
	} else {
		ifm->ifi_flags &= ~IFF_UP;
	}
	if (promisc) {
		ifm->ifi_flags |= IFF_PROMISC;
	} else {
		ifm->ifi_flags &= ~IFF_PROMISC;
	}
	mnl_attr_put_str(nlh, IFLA_IFNAME, sysiface);
	return mnltalk(self, nlh, NULL);
}


bool Netsys_findAllRoutes(Netsys self, const char *iface, NetSysRoute_t *out, int *size)
{
	if (!self) return false;
	if (!iface) return false;
	if (!out || !size || *size <= 0) return false;
	if (!NetwHlpr_interfaceInfo(iface, &self->searchmem.idx, NULL, NULL, NULL)) return false;

	struct nlmsghdr *nlh;
	struct rtmsg *rtm;
	bool ret;

	nlh = mnl_nlmsg_put_header(self->buf);
	nlh->nlmsg_type = RTM_GETROUTE;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	rtm = (struct rtmsg *)mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtmsg));
	rtm->rtm_family = AF_INET;
	self->searchmem.route.it = 0;
	self->searchmem.route.max = *size;
	self->searchmem.route.self = out;
	ret = mnltalk(self, nlh, mnltalk_route_cb);
	*size = self->searchmem.route.it;
	return ret;
}


bool Netsys_findAllIpAddrs(Netsys self, const char *iface, NetSysIpAddr_t *out, int *size)
{
	if (!self) return false;
	if (!iface) return false;
	if (!out || !size || *size <= 0) return false;
	if (!NetwHlpr_interfaceInfo(iface, &self->searchmem.idx, NULL, NULL, NULL)) return false;

	struct nlmsghdr *nlh;
	struct rtgenmsg *rt;
	bool ret;

	nlh = mnl_nlmsg_put_header(self->buf);
	nlh->nlmsg_type	= RTM_GETADDR;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	rt = (struct rtgenmsg *)mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_INET;
	self->searchmem.ipaddr.it = 0;
	self->searchmem.ipaddr.max = *size;
	self->searchmem.ipaddr.self = out;
	ret = mnltalk(self, nlh, mnltalk_ipaddr_cb);
	*size = self->searchmem.ipaddr.it;
	return ret;
}


bool Netsys_findAllIfaces(Netsys self, NetSysIfaceType_e filter, NetSysIface_t *out, int *size)
{
	if (!self) return false;
	if (!out || !size || *size <= 0) return false;

	struct nlmsghdr *nlh;
	struct rtgenmsg *rt;
	bool ret;

	nlh = mnl_nlmsg_put_header(self->buf);
	nlh->nlmsg_type	= RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	rt = (struct rtgenmsg *)mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_PACKET;
	self->searchmem.link.it = 0;
	self->searchmem.link.max = *size;
	self->searchmem.link.self = out;
	self->searchmem.link.ufilter = (uint32_t)filter;
	ret = mnltalk(self, nlh, mnltalk_link_cb);
	*size = self->searchmem.link.it;
	return ret;
}


bool Netsys_cleanupIface(Netsys self, const char *iface)
{
	if (!self) return false;
	if (!iface) return false;
	if (!NetwHlpr_interfaceInfo(iface, &self->searchmem.idx, NULL, NULL, NULL)) return false;

	char destIP[16];
	char destMask[16];
	char srcIP[16];
	char gwIP[16];
	NetSysRoute_t routes[CLEANUP_ROUTE_MAX];
	NetSysIpAddr_t ipaddrs[CLEANUP_IPADDR_MAX];
	int routes_cnt = CLEANUP_ROUTE_MAX;
	int ipaddrs_cnt = CLEANUP_IPADDR_MAX;

	// searching
	if (Netsys_findAllRoutes(self, iface, routes, &routes_cnt) == false) return false;
	if (Netsys_findAllIpAddrs(self, iface, ipaddrs, &ipaddrs_cnt) == false) return false;

	// remove routes
	for (int i = 0; i < routes_cnt; ++i) {
		NetSysRoute_t *route = &routes[i];
		char *pdestIP = NULL;
		char *pdestMask = NULL;
		char *psrcIP = NULL;
		char *pgwIP = NULL;
		if (route->dstip != 0) {
			Hal_ipv4BinToStr(route->dstip, destIP);
			NetwHlpr_prefixToMask(route->dstpfx, destMask);
			pdestIP = destIP;
			pdestMask = destMask;
		}
		if (route->srcip != 0) {
			Hal_ipv4BinToStr(route->srcip, srcIP);
			psrcIP = srcIP;
		}
		if (route->gwip != 0) {
			Hal_ipv4BinToStr(route->gwip, gwIP);
			pgwIP = gwIP;
		}
		Netsys_delIpRoute(self, iface, psrcIP, pdestIP, pdestMask, pgwIP, (int)route->prio);
	}
	// remove ip
	for (int i = 0; i < ipaddrs_cnt; ++i) {
		NetSysIpAddr_t *ipaddr = &ipaddrs[i];
		Hal_ipv4BinToStr(ipaddr->ip, destIP);
		NetwHlpr_prefixToMask(ipaddr->pfx, destMask);
		Netsys_delIpAddr(self, iface, destIP, destMask);
	}

	return true;
}


Netsys Netsys_create(void)
{
	Netsys self;
	struct mnl_socket *nl;
	char *buf;

	nl = mnl_socket_open(NETLINK_ROUTE);
	if (!nl) { return NULL; }

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) != 0) {
		goto nl_exit;
	}

	setSocketNonBlocking(mnl_socket_get_fd(nl));

	buf = (char *)calloc(1, MNL_SOCKET_BUFFER_SIZE);
	if (!buf) {
		goto nl_exit;
	}

	self = (Netsys)calloc(1, sizeof(struct sNetsys));
	if (self) {
		self->nl = nl;
		self->portid = mnl_socket_get_portid(nl);
		self->buf = buf;
		self->seq = time(NULL);
		return self;
	}

	free(buf);
nl_exit:
	mnl_socket_close(nl);
	return NULL;
}

void Netsys_destroy(Netsys self)
{
	if (!self) return;
	if (self->nl) mnl_socket_close(self->nl);
	if (self->buf) free(self->buf);
	free(self);
}

#else // HAL_LIBMNL_SUPPORTED

bool
Netsys_addIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority)
{ return false; }

bool
Netsys_delIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority)
{ return false; }

bool
Netsys_addIpAddr(Netsys self, const char *iface,
		const char *ipaddr, const char *ipmask,
		const char *labelstr, int metric)
{ return false; }

bool
Netsys_delIpAddr(Netsys self, const char *iface,
		const char *ipaddr, const char *ipmask)
{ return false; }

bool
Netsys_controlLink(Netsys self, const char *iface, bool up, bool promisc)
{ return false; }

bool
Netsys_findAllRoutes(Netsys self, const char *iface, NetSysRoute_t *out, int *size)
{ return false; }

bool
Netsys_findAllIpAddrs(Netsys self, const char *iface, NetSysIpAddr_t *out, int *size)
{ return false; }

bool
Netsys_findAllIfaces(Netsys self, NetSysIfaceType_e filter, NetSysIface_t *out, int *size)
{ return false; }

bool
Netsys_cleanupIface(Netsys self, const char *iface)
{ return false; }

Netsys
Netsys_create(void)
{ return NULL; }

void
Netsys_destroy(Netsys self)
{}

#endif // HAL_LIBMNL_SUPPORTED
#endif // __linux__