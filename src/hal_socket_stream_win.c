
#if defined(_WIN32) || defined(_WIN64)

#include "hal_socket_stream.h"
#include "hal_thread.h"
#include "hal_syshelper.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <netioapi.h>
#include <windows.h>


struct sClientSocket {
	SOCKET s;
	int domain;
	bool inreset;
	//
	ServerSocket server;
	int idx;
	struct sServerClient list;
};

struct sClients {
	Mutex mu;
	int size;
	int maxConnections;
	struct sClientSocket *self;
};

struct sServerSocket {
	SOCKET s;
	int domain;
	struct sClients clients;
};


static bool prepareSocketAddress(const char *address, uint16_t port, struct sockaddr_in *sockaddr);


static inline void setSocketNonBlocking(SOCKET s)
{
	u_long val = 1;
	ioctlsocket(s, FIONBIO, &val);
}

static inline int getSocketAvailableToRead(SOCKET s)
{
    u_long val = 0;
	if (ioctlsocket(s, FIONREAD, &val) == 0) {
		return (int)val;
	}
	return -1;
}


ServerSocket TcpServerSocket_create(int maxConnections, const char *address, uint16_t port)
{
	if (address == NULL || port == 0) return NULL;

	HalShSys_init();

	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) return NULL;

	struct sockaddr_in serverAddress;
	Mutex mu = NULL;
	ServerSocket self;
	struct linger lin = {
		.l_onoff = 1,
		.l_linger = 0
	};

	if (!prepareSocketAddress(address, port, &serverAddress)) {
		goto exit_error;
	}
	if (bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) != 0) {
		goto exit_error;
	}

	// disable TIME_WAIT
	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(struct linger)) != 0) goto exit_error;

	mu = Mutex_create();
	if (!mu) {
		goto exit_error;
	}
	self = (ServerSocket)calloc(1, sizeof(struct sServerSocket));
	if (self) {
		self->clients.self = (struct sClientSocket *)calloc(maxConnections, sizeof(struct sClientSocket));
		if (self->clients.self) {
			self->s = sock;
			self->domain = AF_INET;
			self->clients.maxConnections = maxConnections;
			self->clients.mu = mu;
			setSocketNonBlocking(sock);
			return self;
		} else { free(self); }
	}

exit_error:
	if (mu) Mutex_destroy(mu);
	closesocket(sock);
	return NULL;
}


ClientSocket TcpClientSocket_createAndBind(const char *ip, uint16_t port)
{
	HalShSys_init();

	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) return NULL;

	ClientSocket self;
	struct linger lin = {
		.l_onoff = 1,
		.l_linger = 0
	};

	if (ip || port) {
		struct sockaddr_in addr;
		if (!prepareSocketAddress(ip, port, &addr)) {
			goto exit_error;
		}
		if (bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) != 0) {
			goto exit_error;
		}
	}

	// RST instead FIN
	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(struct linger)) != 0) goto exit_error;

	self = (ClientSocket)calloc(1, sizeof(struct sClientSocket));
	if (self) {
		self->s = sock;
		self->domain = AF_INET;
		self->inreset = true;
		self->server = NULL;
		self->idx = -1;
		setSocketNonBlocking(sock);
		return self;
	}

exit_error:
	closesocket(sock);
	return NULL;
}

ClientSocket TcpClientSocket_create()
{
	return TcpClientSocket_createAndBind(0, 0);
}

void TcpClientSocket_activateTcpKeepAlive(ClientSocket self, int idleTime, int interval, int count)
{
	if (self == NULL) return;
	int optval;
	socklen_t optlen = sizeof(optval);
	optval = 1;
	setsockopt(self->s, SOL_SOCKET, SO_KEEPALIVE, (const char *)&optval, optlen);
	optval = idleTime;
	setsockopt(self->s, IPPROTO_TCP, TCP_KEEPIDLE, (const char *)&optval, optlen);
	optval = interval;
	setsockopt(self->s, IPPROTO_TCP, TCP_KEEPINTVL, (const char *)&optval, optlen);
	optval = count;
	setsockopt(self->s, IPPROTO_TCP, TCP_KEEPCNT, (const char *)&optval, optlen);
}

void TcpClientSocket_activateTcpNoDelay(ClientSocket self)
{
	if (self == NULL) return;
	int optval = 1;
	socklen_t optlen = sizeof(optval);
	setsockopt(self->s, IPPROTO_TCP, TCP_NODELAY, (const char *)&optval, optlen);
}

void TcpClientSocket_setUnacknowledgedTimeout(ClientSocket self, int timeoutInMs)
{
	if (self == NULL) return;
}


ServerSocket LocalServerSocket_create(int maxConnections, const char *address)
{
	if (address == NULL) return NULL;

	DeleteFileA(address);

	uint16_t port = Hal_generatePort(address, HAL_LOCAL_SOCK_PORT_MIN, HAL_LOCAL_SOCK_PORT_MAX);
	ServerSocket self = TcpServerSocket_create(maxConnections, HAL_LOCAL_SOCK_ADDR, port);

	if (self) {
		self->domain = AF_UNIX;
	}

	return self;
}


ClientSocket LocalClientSocket_create(void)
{
	HalShSys_init();

	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) return NULL;

	ClientSocket self = (ClientSocket)calloc(1, sizeof(struct sClientSocket));
	if (self) {
		self->s = sock;
		self->domain = AF_UNIX;
		self->inreset = true;
		self->server = NULL;
		self->idx = -1;
		setSocketNonBlocking(sock);
		return self;
	}

// exit_error:
	closesocket(sock);
	return NULL;
}


void ServerSocket_listen(ServerSocket self, int pending)
{
	if (self == NULL) return;
	listen(self->s, pending);
}

ClientSocket ServerSocket_accept(ServerSocket self)
{
	if (self == NULL) return NULL;

	SOCKET sock;

	ClientSocket conSocket = NULL;
	sock = accept(self->s, NULL, NULL);

	if (sock != INVALID_SOCKET) {
		struct sockaddr_storage addr;
		socklen_t addrLen = sizeof(addr);
		if (getsockname(sock, (struct sockaddr*)&addr, &addrLen) != 0) goto exit_error;
		Mutex_lock(self->clients.mu);
		{
			if (self->clients.size >= self->clients.maxConnections) {
				Mutex_unlock(self->clients.mu);
				goto exit_error;
			}
			for (int i = 0; i < self->clients.maxConnections; ++i) {
				if ( self->clients.self[i].server == NULL ) {
					conSocket = &(self->clients.self[i]);
					conSocket->s = sock;
					conSocket->domain = addr.ss_family;
					conSocket->inreset = false;
					conSocket->server = self;
					conSocket->idx = i;
					self->clients.size++;
					break;
				}
			}
		}
		Mutex_unlock(self->clients.mu);
	}

	return conSocket;

exit_error:
	closesocket(sock);
	return NULL;
}

int ServerSocket_getClientsNumber(ServerSocket self)
{
	if (self == NULL) return 0;
	Mutex_lock(self->clients.mu);
	int cnt = self->clients.size;
	Mutex_unlock(self->clients.mu);
	return cnt;
}

ServerClient ServerSocket_getClients(ServerSocket self)
{
	ServerClient ret = NULL;
	ServerClient it = NULL;
	if (self == NULL) return NULL;
	Mutex_lock(self->clients.mu);
	if (self->clients.size > 0) {
		for (int i = 0; i < self->clients.maxConnections; ++i) {
			ClientSocket c = &(self->clients.self[i]);
			if (c->server != NULL) {
				if (it) it->next = &(c->list);
				it = &(c->list);
				it->self = c;
				it->next = NULL;
				if (ret == NULL) ret = it;
			}
		}
	}
	Mutex_unlock(self->clients.mu);
	return ret;
}

static void ServerSocket_deleteClient(ServerSocket self, ClientSocket client)
{
	if (self == NULL || client == NULL) return;
	if (client->idx >= 0) {
		Mutex_lock(self->clients.mu);
		{
			client->inreset = true;
			client->server = NULL;
			client->idx = -1;
			self->clients.size--;
		}
		Mutex_unlock(self->clients.mu);
	}
}

static inline void closeAndShutdownSocket(SOCKET sock)
{
	if (sock != INVALID_SOCKET) {
		shutdown(sock, SD_BOTH);
		closesocket(sock);
	}
}

void ServerSocket_closeClients(ServerSocket self)
{
	Mutex_lock(self->clients.mu);
	if (self->clients.size > 0) {
		for (int i = 0; i < self->clients.maxConnections; ++i) {
			ClientSocket c = &(self->clients.self[i]);
			if (c->server != NULL) {
				ServerSocket_deleteClient(self, c);
			}
		}
	}
	Mutex_unlock(self->clients.mu);
}

bool ServerSocket_destroy(ServerSocket self)
{
	if (self == NULL) return false;
	if (self->clients.size != 0) return false;
	Mutex_destroy(self->clients.mu);
	free(self->clients.self);
	closeAndShutdownSocket(self->s);
	self->s = INVALID_SOCKET;
	HalShSys_deinit();
	free(self);
	return true;
}

unidesc ServerSocket_getDescriptor(ServerSocket self)
{
	if (self) {
		unidesc ret;
		ret.u64 = (uint64_t)self->s;
		return ret;
	}
	return Hal_getInvalidUnidesc();
}


bool ClientSocket_connectAsync(ClientSocket self, const ClientSocketAddress address)
{
	if (self == NULL || address == NULL) return false;
	if (self->server) return false;

	struct sockaddr_in serverAddress;

	switch (self->domain) {
		case AF_INET: {
			if (!prepareSocketAddress(address->ip, address->port, &serverAddress))
				return false;
			if (connect(self->s, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
				if (errno != EINPROGRESS) {
					return false;
				}
			}
		} break;
		case AF_UNIX: {
			uint16_t port = Hal_generatePort(address->address, HAL_LOCAL_SOCK_PORT_MIN, HAL_LOCAL_SOCK_PORT_MAX);
			if (!prepareSocketAddress(HAL_LOCAL_SOCK_ADDR, port, &serverAddress))
				return false;
			if (connect(self->s, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
				return false;
			}
		} break;
	}

	self->inreset = false;
	return true; /* is connecting or already connected */
}

bool ClientSocket_connect(ClientSocket self, const ClientSocketAddress address, uint32_t timeoutInMs)
{
	if (self == NULL || address == NULL) return false;
	if (self->server) return false;

	if (ClientSocket_connectAsync(self, address) == false)
		return false;

	struct timeval timeout;
	timeout.tv_sec = timeoutInMs / 1000;
	timeout.tv_usec = (timeoutInMs % 1000) * 1000;

	fd_set fdSet;
	FD_ZERO(&fdSet);
	FD_SET(self->s, &fdSet);

	if (select(0, NULL, &fdSet , NULL, &timeout) == 1) {
		int so_error;
		socklen_t len = sizeof so_error;
		if (getsockopt(self->s, SOL_SOCKET, SO_ERROR, (char *)&so_error, &len) != 0) {
			if (so_error == 0) {
				return true;
			}
		}
	}

	self->inreset = true;
	return false;
}

void ClientSocket_close(ClientSocket self) // hal internal use only
{
	if (self == NULL) return;
	if (self->s != INVALID_SOCKET) {
		closeAndShutdownSocket(self->s);
		self->s = INVALID_SOCKET;
		self->inreset = true;
	}
}

bool ClientSocket_reset(ClientSocket self)
{
	if (self == NULL) return false;
	if (self->s != INVALID_SOCKET && self->server == NULL) {
		closeAndShutdownSocket(self->s);
		self->s = socket(AF_INET, SOCK_STREAM, 0);
		self->inreset = true;
		if (self->s != INVALID_SOCKET) {
			return true;
		}
	}
	return false;
}

ClientSocketState ClientSocket_checkConnectState(ClientSocket self)
{
	if (self == NULL) return SOCKET_STATE_ERROR_UNKNOWN;
	fd_set fdSet;
	struct timeval timeout;

	memset(&timeout, 0, sizeof(struct timeval));

	FD_ZERO(&fdSet);
	FD_SET(self->s, &fdSet);

	int selectVal = select(0, NULL, &fdSet , NULL, &timeout);

	if (selectVal == 1) {
		/* Check if connection is established */
		int so_error;
		socklen_t len = sizeof(so_error);
		if (getsockopt(self->s, SOL_SOCKET, SO_ERROR, (char *)&so_error, &len) != 0) {
			if (so_error == 0)
				return (self->inreset)? SOCKET_STATE_IDLE : SOCKET_STATE_CONNECTED;
		}
		return SOCKET_STATE_FAILED;
	} else if (selectVal == 0) {
		return SOCKET_STATE_CONNECTING;
	} else {
		return SOCKET_STATE_FAILED;
	}
}

int ClientSocket_read(ClientSocket self, uint8_t *buf, int size)
{
	if (self == NULL || buf == NULL) return -1;

	if (self->s == INVALID_SOCKET)
		return -1;

	int read_bytes = recv(self->s, (char *)buf, size, 0); // todo

	if (read_bytes == 0)
		return 0;

	if (read_bytes < 0) {
		int error = WSAGetLastError();
		switch (error) {
			case WSAEWOULDBLOCK: return 0;
			default: return -1;
		}
	}

	return read_bytes;
}

int ClientSocket_write(ClientSocket self, const uint8_t *buf, int size)
{
	if (self == NULL || buf == NULL) return -1;

	if (self->s == INVALID_SOCKET)
		return -1;

	int retVal = send(self->s, (const char *)buf, size, 0); // todo

	if (retVal <= 0) {
		int error = WSAGetLastError();
		if (error == EWOULDBLOCK) {
			return 0;
		} else {
			return -1;
		}
	}

	return retVal;
}

static bool convertAddressToStr(struct sockaddr_storage *addr, ClientSocketAddress address)
{
	switch (addr->ss_family) {
		case AF_INET: {
			struct sockaddr_in *paddr = (struct sockaddr_in*)addr;
			address->port = ntohs(paddr->sin_port);
			inet_ntop(AF_INET, &(paddr->sin_addr), address->ip, INET_ADDRSTRLEN);
		} break;
		default: return false;
	}
	return true;
}

bool ClientSocket_getPeerAddress(ClientSocket self, ClientSocketAddress address)
{
	if (self == NULL || address == NULL) return false;
	struct sockaddr_storage addr;
	socklen_t addrLen = sizeof(addr);
	if (getpeername(self->s, (struct sockaddr*) &addr, &addrLen) == 0) {
		return convertAddressToStr(&addr, address);
	}
	return false;
}

bool ClientSocket_getLocalAddress(ClientSocket self, ClientSocketAddress address)
{
	if (self == NULL || address == NULL) return false;
	struct sockaddr_storage addr;
	socklen_t addrLen = sizeof(addr);
	if (getsockname(self->s, (struct sockaddr*) &addr, &addrLen) == 0) {
		return convertAddressToStr(&addr, address);
	}
	return false;
}

int ClientSocket_readAvailable(ClientSocket self)
{
	if (self == NULL) return -1;
	return getSocketAvailableToRead(self->s);
}

void ClientSocket_destroy(ClientSocket self)
{
	if (self == NULL) return;
	closeAndShutdownSocket(self->s);
	self->s = INVALID_SOCKET;
	if (self->server) {
		ServerSocket_deleteClient(self->server, self);
	} else {
		HalShSys_deinit();
		free(self);
	}
}

unidesc ClientSocket_getDescriptor(ClientSocket self)
{
	if (self) {
		unidesc ret;
		ret.u64 = (uint64_t)self->s;
		return ret;
	}
	return Hal_getInvalidUnidesc();
}


static bool prepareSocketAddress(const char *address, uint16_t port, struct sockaddr_in *sockaddr)
{
	bool retVal = true;

	memset((char *) sockaddr, 0, sizeof(struct sockaddr_in));

	if (address != NULL) {
		struct addrinfo addressHints;
		struct addrinfo *lookupResult;
		int result;

		memset(&addressHints, 0, sizeof(struct addrinfo));
		addressHints.ai_family = AF_INET;
		result = getaddrinfo(address, NULL, &addressHints, &lookupResult);

		if (result != 0) {
			return false;
		}

		memcpy(sockaddr, lookupResult->ai_addr, sizeof(struct sockaddr_in));
		freeaddrinfo(lookupResult);
	} else {
		sockaddr->sin_addr.s_addr = htonl(INADDR_ANY);
	}

	sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons(port);

	return retVal;
}


#endif // _WIN32 || _WIN64