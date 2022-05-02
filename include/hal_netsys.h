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


/** Opaque reference of a Netsys instance */
typedef struct sNetsys *Netsys;


typedef struct {
	uint32_t dstip;	// destination network address
	uint8_t dstpfx;	// destination network prefix (mask)
	uint32_t gwip;	// gateway
	uint32_t prio;	// route priority (metric)
	uint32_t srcip;	// output system ip address (maybe unused)
} NetSysRoute_t;

typedef struct {
	uint32_t ip;	// ip address 
	uint8_t pfx;	// network prefix (mask)
} NetSysIpAddr_t;

typedef enum {
	NSIT_Any		= 0,
	NSIT_Other 		= 0x1,
	NSIT_Ether 		= 0x2,
	NSIT_Wifi 		= 0x4,
	NSIT_Loopback 	= 0x8,
} NetSysIfaceType_e;

typedef struct {
	char idx[6];	// interface numerical index in c-string representation
	NetSysIfaceType_e type; // one of NSIT_Other+
} NetSysIface_t;


/**
 * \brief Add new ip static route to the system
 *
 * \param iface the ID of the Ethernet interface (number, name or ip address on the interface)
 * \param srcIP output system ip address (maybe unused)
 * \param destIP destination network address (ex 10.10.10.0)
 * \param destMask destination network mask (ex 255.255.255.0)
 * \param gwIP gateway
 * \param priority route priority (metric)
 *
 * \return true on success, false on error
 */
HAL_API bool
Netsys_addIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority);

/**
 * \brief Delete the specified ip static route from the system
 *
 * \param iface the ID of the Ethernet interface (number, name or ip address on the interface)
 * \param srcIP output system ip address (maybe unused)
 * \param destIP destination network address (ex 10.10.10.0)
 * \param destMask destination network mask (ex 255.255.255.0)
 * \param gwIP gateway
 * \param priority route priority (metric)
 *
 * \return true on success, false on error
 */
HAL_API bool
Netsys_delIpRoute(Netsys self, const char *iface,
		const char *srcIP,
		const char *destIP, const char *destMask,
		const char *gwIP,
		int priority);

/**
 * \brief Add new static ip address to the system
 *
 * \param iface the ID of the Ethernet interface (number, name or ip address on the interface)
 * \param ipaddr ip address (ex 10.10.10.1)
 * \param ipmask ip mask (ex 255.255.255.0)
 * \param labelstr address label (maybe unused)
 * \param priority priority (metric) of the accompanying route (maybe unused)
 *
 * \return true on success, false on error
 */
HAL_API bool
Netsys_addIpAddr(Netsys self, const char *iface,
		const char *ipaddr, const char *ipmask,
		const char *labelstr, int metric);

/**
 * \brief Delete the specified static ip address from the system
 *
 * \param iface the ID of the Ethernet interface (number, name or ip address on the interface)
 * \param ipaddr ip address (ex 10.10.10.1)
 * \param ipmask ip mask (ex 255.255.255.0)
 *
 * \return true on success, false on error
 */
HAL_API bool
Netsys_delIpAddr(Netsys self, const char *iface,
		const char *ipaddr, const char *ipmask);

/**
 * \brief Control the system link
 *
 * \param iface the ID of the Ethernet interface (number, name or ip address on the interface)
 * \param up enable or disable administrative link status
 * \param promisc enable or disable promiscuous link mode
 *
 * \return true on success, false on error
 */
HAL_API bool
Netsys_controlLink(Netsys self, const char *iface, bool up, bool promisc);

/**
 * \brief Find all routes on the specified interface
 *
 * \param iface the ID of the Ethernet interface (number, name or ip address on the interface)
 * \param out output data storage
 * \param size size of (in elements) of the output storage
 *
 * \return true on success, false on error
 */
HAL_API bool
Netsys_findAllRoutes(Netsys self, const char *iface, NetSysRoute_t *out, int *size);

/**
 * \brief Find all ip addresses on the specified interface
 *
 * \param iface the ID of the Ethernet interface (number, name or ip address on the interface)
 * \param out output data storage
 * \param size size of (in elements) of the output storage
 *
 * \return true on success, false on error
 */
HAL_API bool
Netsys_findAllIpAddrs(Netsys self, const char *iface, NetSysIpAddr_t *out, int *size);

/**
 * \brief Find all system interfaces
 *
 * \param filter bit field searching filter
 * \param out output data storage
 * \param size size of (in elements) of the output storage
 *
 * \return true on success, false on error
 */
HAL_API bool
Netsys_findAllIfaces(Netsys self, NetSysIfaceType_e filter, NetSysIface_t *out, int *size);

/**
 * \brief Delete all routes and ip addresses from the specified interface
 *
 * \param iface the ID of the Ethernet interface (number, name or ip address on the interface)
 *
 * \return true on success, false on error
 */
HAL_API bool
Netsys_cleanupIface(Netsys self, const char *iface);

/**
 * \brief Create Netsys instance
 *
 * \return a new Netsys instance.
*/
HAL_API Netsys
Netsys_create(void);

/**
 * \brief Destroy Netsys instance
*/
HAL_API void
Netsys_destroy(Netsys self);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif


#endif /* HAL_NETSYS_H */