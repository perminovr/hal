#ifndef HAL_SOCKET_DGRAM_H
#define HAL_SOCKET_DGRAM_H

#include "hal_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/*! \defgroup hal Platform (Hardware/OS) abstraction layer
   *
   *  @{
   */

/**
 * @defgroup HAL_SOCKET_DGRAM Interface to the datagramm sockets (UDP, Local, Ethernet) (abstract socket layer)
 *
 * @{
 */


/** Opaque reference for a socket instance */
typedef struct sDgramSocket *DgramSocket;

/** Opaque reference for a socket instance */
typedef union uDgramSocketAddress *DgramSocketAddress;


union uDgramSocketAddress {
	struct {
		char ip[16];
		uint16_t port;
	};
	uint8_t mac[6];
	char address[64];
};


/**
 * @defgroup HAL_SOCKET_STREAM_PROT_SPEC Protocol specific API
 *
 * @{
 */


/**
 * \brief Create a UDP socket and bind to system socket
 *
 * \param ip ip v4/v6 address or hostname
 * \param port udp port
 *
 * \return a new udp socket instance.
 */
HAL_API DgramSocket
UdpDgramSocket_createAndBind(const char *ip, uint16_t port);

/**
 * \brief Create a UDP socket without bind (any address)
 *
 * \return a new socket instance.
 */
HAL_API DgramSocket
UdpDgramSocket_create(void);

/**
 * \brief Bind a UDP socket to system socket
 *
 * \param self the socket instance
 * \param ip ip v4/v6 address or hostname
 * \param port udp port
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool
UdpDgramSocket_bind(DgramSocket self, const char *ip, uint16_t port);

/**
 * \brief The socket joins to the UDP group (multicast)
 *
 * \param self the socket instance
 * \param ip ip v4 of the group
 * \param iface the ID of the Ethernet interface
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool
UdpDgramSocket_joinGroup(DgramSocket self, const char *ip, const char *iface);

/**
 * \brief Set reuse option for the socket
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool
UdpDgramSocket_setReuse(DgramSocket self, bool reuse);


/**
 * \brief Create a local socket
 *
 * \param address the address of the local socket
 *
 * \return a new socket instance.
 */
HAL_API DgramSocket
LocalDgramSocket_create(const char *address);

/**
 * \brief Unlink old local socket
 *
 * \param address the address of the local socket
 */
HAL_API void
LocalDgramSocket_unlinkAddress(const char *address);


/**
 * \brief Create a ethernet socket
 *
 * \param iface the ID of the Ethernet interface
 * \param ethTypeFilter ether type receiving filter. If parameter equals to zero,
 * when no filter is applied (all packets will be received)
 *
 * \return a new socket instance.
 */
HAL_API DgramSocket
EtherDgramSocket_create(const char *iface, uint16_t ethTypeFilter);

/**
 * \brief Set ethernet frame header
 *
 * \param header storage for header. At least 14 byte length
 * \param src source mac address
 * \param dst destination mac address
 * \param ethType ethernet frame type
 *
 * \return header len (always 14)
 */
HAL_API int
EtherDgramSocket_setHeader(uint8_t *header, const uint8_t *src, const uint8_t *dst, uint16_t ethType);

/**
 * \brief Get ethernet frame header from buffer
 *
 * \param header raw ethernet header. At least 14 bytes length
 * \param src storage for source mac address. At least 6 bytes length
 * \param dst storage fordestination mac address. At least 6 bytes length
 * \param ethType storage for ethernet frame type
 */
HAL_API void
EtherDgramSocket_getHeader(const uint8_t *header, uint8_t *src, uint8_t *dst, uint16_t *ethType);

/**
 * \brief Return the MAC address of an Ethernet interface.
 *
 * The result are the six bytes that make up the Ethernet MAC address.
 *
 * \param iface the ID of the Ethernet interface
 * \param addr pointer to a buffer to store the MAC address. At least 6 bytes length
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool
EtherDgramSocket_getInterfaceMACAddress(const char *iface, uint8_t *addr);

/**
 * \brief Return the MAC address of the socket.
 *
 * The result are the six bytes that make up the Ethernet MAC address.
 *
 * \param self the socket instance
 * \param addr pointer to a buffer to store the MAC address. At least 6 bytes length
 *
 * \return pointer to addr in case of success, NULL otherwise
 */
HAL_API uint8_t *
EtherDgramSocket_getSocketMACAddress(DgramSocket self, uint8_t *addr);


/*! @} */


/**
 * \brief Reset system socket (close and open)
 *
 * \param self the socket instance
 * \return true in case of success, false otherwise
 */
HAL_API bool
DgramSocket_reset(DgramSocket self);

/**
 * \brief Set remote partner for massage exchanging (read/write filter)
 *
 * \param self the socket instance
 * \param addr destination address (protocol specific)
 */
HAL_API void
DgramSocket_setRemote(DgramSocket self, const DgramSocketAddress addr);

/**
 * \brief Get remote partner installed thru DgramSocket_setRemote
 *
 * \param self the socket instance
 * \param addr destination address (protocol specific)
 */
HAL_API void
DgramSocket_getRemote(DgramSocket self, DgramSocketAddress addr);

/**
 * \brief Read from socket to local buffer (non-blocking) from remote partner only
 *
 * The function shall return immediately if no data is available. In this case
 * the function returns 0. If an error happens the function shall return -1.
 *
 * Remote address must be assigned via \ref DgramSocket_setRemote for filterring
 * purpose. If the address is not defined when use \ref DgramSocket_readFrom instead.
 *
 * \param self the socket instance
 * \param buf the buffer where the read bytes are copied to
 * \param size the maximum number of bytes to read (size of the provided buffer)
 *
 * \return the number of bytes read or -1 if an error occurred
 */
HAL_API int
DgramSocket_read(DgramSocket self, uint8_t *buf, int size);

/**
 * \brief Send a message through the socket to remote partner
 *
 * Destination address must be assigned via \ref DgramSocket_setRemote.
 *
 * \param self the socket instance
 * \param buf data to send
 * \param size size of data
 *
 * \return number of bytes transmitted of -1 in case of an error
 */
HAL_API int
DgramSocket_write(DgramSocket self, const uint8_t *buf, int size);

/**
 * \brief Read from socket to local buffer (non-blocking)
 *
 * The function shall return immediately if no data is available. In this case
 * the function returns 0. If an error happens the function shall return -1.
 *
 * \param self the socket instance
 * \param addr address of data source  (protocol specific).
 * \param buf the buffer where the read bytes are copied to
 * \param size the maximum number of bytes to read (size of the provided buffer)
 *
 * \return the number of bytes read or -1 if an error occurred
 */
HAL_API int
DgramSocket_readFrom(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size);

/**
 * \brief Send a message through the socket
 *
 * \param self the socket instance
 * \param addr destination address (protocol specific).
 * \param buf data to send
 * \param size size of data
 *
 * \return number of bytes transmitted of -1 in case of an error
 */
HAL_API int
DgramSocket_writeTo(DgramSocket self, const DgramSocketAddress addr, const uint8_t *buf, int size);

/**
 * \brief Get the number of bytes available for reading from the socket
 *
 * \param self the socket instance
 *
 * \return the number of bytes available for reading or -1 if an error occurred
 */
HAL_API int
DgramSocket_readAvailable(DgramSocket self);

/**
 * \brief destroy a socket (after close the socket)
 *
 * This function shall close the connection (if one is established) and free all
 * resources allocated by the socket.
 *
 * \param self the socket instance
 */
HAL_API void
DgramSocket_destroy(DgramSocket self);

/**
 * \brief Get the system descriptor
 */
HAL_API unidesc
DgramSocket_getDescriptor(DgramSocket self);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif


#endif /* HAL_SOCKET_DGRAM_H */