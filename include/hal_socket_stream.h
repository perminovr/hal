#ifndef HAL_SOCKET_STREAM_H
#define HAL_SOCKET_STREAM_H


#include "hal_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/*! \defgroup hal Platform (Hardware/OS) abstraction layer
   *
   *  @{
   */

/**
 * @defgroup HAL_SOCKET_STREAM Interface to the stream sockets (TCP/IP, Local) (abstract socket layer)
 *
 * @{
 */


/** Opaque reference for a server socket instance */
typedef struct sServerSocket *ServerSocket;

/** Opaque reference for a client or connection socket instance */
typedef struct sClientSocket *ClientSocket;

/** State of an asynchronous connect */
typedef enum {
	SOCKET_STATE_IDLE = 0,
	SOCKET_STATE_CONNECTING,
	SOCKET_STATE_CONNECTED,
	SOCKET_STATE_FAILED,
	SOCKET_STATE_ERROR_UNKNOWN = 99
} ClientSocketState;

/** Opaque reference for a client instance of a server */
typedef struct sServerClient *ServerClient;

struct sServerClient {
	ClientSocket self;
	struct sServerClient *next;		// is NULL for last client
};

/** Opaque reference for a socket instance */
typedef union uClientSocketAddress *ClientSocketAddress;

union uClientSocketAddress {
	struct {
		char ip[16];
		uint16_t port;
	};
	char address[64];
};


/**
 * @defgroup HAL_SOCKET_STREAM_PROT_SPEC Protocol specific API
 *
 * @{
 */


/**
 * \brief Create a new ServerSocket instance
 *
 * \param maxConnections maximum clients for this server
 * \param address ip address or hostname to listen on
 * \param port the TCP port to listen on
 *
 * \return the newly create ServerSocket instance
 */
HAL_API ServerSocket
TcpServerSocket_create(int maxConnections, const char *address, uint16_t port);

/**
 * \brief Create a TCP client socket and bind to system socket
 *
 * \param ip ip v4/v6 address or hostname
 * \param port tcp port
 *
 * \return a new client socket instance.
 */
HAL_API ClientSocket
TcpClientSocket_createAndBind(const char *ip, uint16_t port);

/**
 * \brief Create a TCP client socket without bind (any address)
 *
 * \return a new client socket instance.
 */
HAL_API ClientSocket
TcpClientSocket_create(void);

/**
 * \brief Activate TCP keep alive for socket and set keep alive parameters
 *
 * \param self server socket instance
 * \param idleTime time (in s) between last received message and first keep alive message
 * \param interval time (in s) between subsequent keep alive messages if no ACK received
 * \param count number of not missing keep alive ACKs until socket is considered dead
 */
HAL_API void
TcpClientSocket_activateTcpKeepAlive(ClientSocket self, int idleTime, int interval, int count);

/**
 * \brief Activate TCP no delay when send frame
 *
 * \param self server socket instance
 */
HAL_API void
TcpClientSocket_activateTcpNoDelay(ClientSocket self);

/**
 * \brief When the value is greater than 0, it specifies the maximum
 *  amount of time in milliseconds that transmitted data may
 *  remain unacknowledged, or bufferred data may remain
 *  untransmitted (due to zero window size) before TCP will
 *  forcibly close the corresponding connection
 *
 * \param self server socket instance
 * \param timeoutInMs the timeout in ms
 */
HAL_API void
TcpClientSocket_setUnacknowledgedTimeout(ClientSocket self, int timeoutInMs);


/**
 * \brief Create a new ServerSocket instance
 *
 * \param maxConnections maximum clients for this server
 * \param address local address name
 *
 * \return the newly create ServerSocket instance
 */
HAL_API ServerSocket
LocalServerSocket_create(int maxConnections, const char *address);

/**
 * \brief Unlink old local socket
 *
 * \param address the address of the local socket
 */
HAL_API void
LocalServerSocket_unlinkAddress(const char *address);

/**
 * \brief Create a local client socket
 *
 * \return a new client socket instance.
 */
HAL_API ClientSocket
LocalClientSocket_create(void);


/*! @} */


/**
 * \brief Start listen on the server socket for clients
 *
 * \param pending size of connection queue (may be unused)
 */
HAL_API void
ServerSocket_listen(ServerSocket self, int pending);

/**
 * \brief Accept a new incoming connection (non-blocking)
 *
 * This function shall accept a new incoming connection. It is non-blocking and has to
 * return NULL if no new connection is pending.
 *
 * \param self server socket instance
 *
 * \return handle of the new connection socket or NULL if no new connection is available
 */
HAL_API ClientSocket
ServerSocket_accept(ServerSocket self);

/**
 * \brief Get the number of the server active clients
 *
 * \param self server socket instance
 *
 * \return number of server clients
 */
HAL_API int
ServerSocket_getClientsNumber(ServerSocket self);

/**
 * \brief Get the list of the server active clients
 *
 * \param self server socket instance
 *
 * \return head of the clients list
 */
HAL_API ServerClient
ServerSocket_getClients(ServerSocket self);

/**
 * \brief Close all static clients soket related this server
 *
 * \param self server socket instance
 */
HAL_API void
ServerSocket_closeClients(ServerSocket self);

/**
 * \brief Destroy a server socket instance
 *
 * Free all resources allocated by this server socket instance.
 *
 * All static clients soket related this server socket must be closed!
 *
 * \param self server socket instance
 *
 * \return true in case of success, false otherwise (there are the active clients)
 */
HAL_API bool
ServerSocket_destroy(ServerSocket self);

/**
 * \brief Get the system descriptor
 */
HAL_API unidesc
ServerSocket_getDescriptor(ServerSocket self);



/**
 * \brief Connect to a server
 *
 * Connect to a server application identified by the address
 *
 * \param self the client socket instance
 * \param address remote server address (protocol specific)
 * \param timeoutInMs the timeout in ms
 *
 * \return true if the connection was established successfully, false otherwise
 */
HAL_API bool
ClientSocket_connect(ClientSocket self, const ClientSocketAddress address, uint32_t timeoutInMs);

/**
 * \brief Connect to a server
 *
 * Connect to a server application identified by the address
 *
 * \param self the client socket instance
 * \param address remote server address (protocol specific)
 *
 * \return true if on connecting state or the connection was established successfully, false otherwise
 */
HAL_API bool
ClientSocket_connectAsync(ClientSocket self, const ClientSocketAddress address);

/**
 * \brief Reset system socket (close and open)
 *
 * Operation will failed if this socket is the socket of a server or if this socket is not valid
 *
 * \param self the client socket instance
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool
ClientSocket_reset(ClientSocket self);

/**
 * \brief Check socket connection state
 *
 * \param self the client socket instance
 */
HAL_API ClientSocketState
ClientSocket_checkConnectState(ClientSocket self);

/**
 * \brief Read from socket to local buffer (non-blocking)
 *
 * The function shall return immediately if no data is available. In this case
 * the function returns 0. If an error happens the function shall return -1.
 *
 * \param self the client socket instance
 * \param buf the buffer where the read bytes are copied to
 * \param size the maximum number of bytes to read (size of the provided buffer)
 *
 * \return the number of bytes read or -1 if an error occurred
 */
HAL_API int
ClientSocket_read(ClientSocket self, uint8_t *buf, int size);

/**
 * \brief Send a message through the socket
 *
 * \param self the client socket instance
 * \param buf data to send
 * \param size size of data
 *
 * \return number of bytes transmitted of -1 in case of an error
 */
HAL_API int
ClientSocket_write(ClientSocket self, const uint8_t *buf, int size);

/**
 * \brief Get the address of the peer application (IP address and port number)
 *
 * \param self the client socket instance
 * \param address remote server address (protocol specific)
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool
ClientSocket_getPeerAddress(ClientSocket self, ClientSocketAddress address);

/**
 * \brief Get the address of the this application (IP address and port number)
 *
 * \param self the client socket instance
 * \param address local address (protocol specific)
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool
ClientSocket_getLocalAddress(ClientSocket self, ClientSocketAddress address);

/**
 * \brief Get the number of bytes available for reading from the socket
 *
 * \param self the socket instance
 *
 * \return the number of bytes available for reading or -1 if an error occurred
 */
HAL_API int
ClientSocket_readAvailable(ClientSocket self);

/**
 * \brief destroy a socket (close the socket if a connection is established)
 *
 * This function shall close the connection (if one is established) and free all
 * resources allocated by the socket.
 *
 * \param self the client socket instance
 */
HAL_API void
ClientSocket_destroy(ClientSocket self);

/**
 * \brief Get the system descriptor
 */
HAL_API unidesc
ClientSocket_getDescriptor(ClientSocket self);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif


#endif /* HAL_SOCKET_STREAM_H */
