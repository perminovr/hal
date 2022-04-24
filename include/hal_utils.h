#ifndef HAL_UTILS_H
#define HAL_UTILS_H


#include "hal_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * \file hal_utils.h
 * \brief 
 */

/*! \addtogroup hal
   *
   *  @{
   */

/**
 * @defgroup HAL_UTILS
 *
 * @{
 */

typedef struct {
	union {
		uint32_t h[4];
		uint8_t digest[16];
	};
	union {
		uint32_t x[16];
		uint8_t buffer[64];
	};
	size_t size;
	uint64_t totalSize;
} Md5HashContext;

/**
 * \brief 
 */
HAL_API void
md5hash_init(Md5HashContext *ctx);

/**
 * \brief 
 */
HAL_API void
md5hash_update(Md5HashContext *ctx, const void *data, size_t length);

/**
 * \brief 
 */
HAL_API void
md5hash_final(Md5HashContext *ctx, uint8_t *digest);

/**
 * \brief Return the MAC address of an Ethernet interface.
 *
 * The result are the six bytes that make up the Ethernet MAC address.
 *
 * \param iface the ID of the Ethernet interface (number, name or ip address on the interface)
 * \param addr pointer to a buffer to store the MAC address. At least 6 bytes length
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool
NetwHlpr_getInterfaceMACAddress(const char *iface, uint8_t *addr);

/**
 * \brief Get interface info by identification
 * 
 * \param iface - identification. Could be:
 *      1) interface number (string)
 *      2) interface name (system/adapter)
 *      3) IP address on the interface
 * 
 * \param num - storage for interface number (if not NULL)
 * \param name - storage for interface name (if not NULL)
 * \param name2 - storage for interface second name (if not NULL and if exists - or copy of name)
 * \param addr - storage for first interface ipv4 address (if not NULL)
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool 
NetwHlpr_interfaceInfo(const char *iface, int *num, char *name, char *name2, char *addr);

/**
 * \brief 
 */
HAL_API uint8_t 
NetwHlpr_maskToPrefix(const char *strMask);

/**
 * \brief 
 */
HAL_API char *
NetwHlpr_prefixToMask(int prefix, char *buf);

/**
 * \brief 
 */
HAL_API uint32_t
NetwHlpr_broadcast(const char *ip, uint8_t prefix);

/**
 * \brief 
 */
HAL_API uint32_t
NetwHlpr_subnet(const char *ip, uint8_t prefix);

/**
 * \brief Generate socket port by name
 * 
 * \param name C-string
 * \param min the minimum value of the socket port
 * \param max the maximum value of the socket port
 * 
 * \details val=(max-min) should be higher than 999
 *
 * \return socket port on success or 0 on failure
 */
HAL_API uint16_t
NetwHlpr_generatePort(const char *name, uint16_t min, uint16_t max);

/**
 * \brief Convert string ipv4 representation to binary
 *
 * \return binary ip data in network byte order
 */
HAL_API uint32_t
Hal_ipv4StrToBin(const char *ip);

/**
 * \brief Convert binary ipv4 representation to string
 *
 * \return pointer to output on success or NULL on failure
 */
HAL_API char *
Hal_ipv4BinToStr(uint32_t addr, char *output);

/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif

#endif // HAL_UTILS_H