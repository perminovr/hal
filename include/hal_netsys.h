#ifndef HAL_NETSYS_H
#define HAL_NETSYS_H


#include "hal_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * \file hal_netsys.h
 * \brief Abstraction layer for system networking
 */

/*! \addtogroup hal
   *
   *  @{
   */

/**
 * @defgroup HAL_NETSYS Netsys API
 *
 * @{
 */


typedef struct {
	uint32_t dstip;
	uint8_t dstpfx;
	uint32_t gwip;
	uint32_t prio;
	uint32_t srcip;
} NetSysRoute_t;

typedef struct {
	uint32_t ip;
	uint8_t pfx;
} NetSysIpAddr_t;


/** Opaque reference of a Netsys instance */
typedef struct sNetsys *Netsys;

HAL_API bool
Netsys_addIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority);

HAL_API bool
Netsys_delIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority);

HAL_API bool
Netsys_addIpAddr(Netsys self, const char *iface,
		const char *ipaddr, const char *ipmask,
		const char *labelstr, int metric);

HAL_API bool
Netsys_delIpAddr(Netsys self, const char *iface,
		const char *ipaddr, const char *ipmask);

HAL_API bool
Netsys_controlLink(Netsys self, const char *iface, bool up, bool promisc);

HAL_API bool
Netsys_findAllRoutes(Netsys self, const char *iface, NetSysRoute_t *out, int *size);

HAL_API bool
Netsys_findAllIpAddrs(Netsys self, const char *iface, NetSysIpAddr_t *out, int *size);

HAL_API bool
Netsys_cleanupIface(Netsys self, const char *iface);

HAL_API Netsys
Netsys_create(void);

HAL_API void
Netsys_destroy(Netsys self);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif


#endif /* HAL_NETSYS_H */