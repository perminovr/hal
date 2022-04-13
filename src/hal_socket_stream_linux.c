
#ifdef __linux__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "hal_socket_stream.h"
#include "hal_thread.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <linux/uinput.h>
#include <linux/version.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

// our kernel supports it
#define TCP_USER_TIMEOUT	18


struct sClientSocket {
	int fd;
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
	int fd;
	int domain;
	struct sClients clients;
};


static bool prepareSocketAddress(const char *address, uint16_t port, struct sockaddr_in *sockaddr);


static inline void setSocketNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static inline int getSocketAvailableToRead(int fd)
{
    int val = 0;
	if (ioctl(fd, FIONREAD, &val) == 0) {
		return val;
	}
	return -1;
}


ServerSocket TcpServerSocket_create(int maxConnections, const char *address, uint16_t port)
{
	if (address == NULL || port == 0) return NULL;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return NULL;

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
	if (bind(fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
		goto exit_error;
	}

	// disable TIME_WAIT
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(struct linger)) < 0) goto exit_error;

	mu = Mutex_create();
	if (!mu) {
		goto exit_error;
	}

	self = (ServerSocket)calloc(1, sizeof(struct sServerSocket));
	if (self) {
		self->clients.self = (struct sClientSocket *)calloc(maxConnections, sizeof(struct sClientSocket));
		if (self->clients.self) {
			self->fd = fd;
			self->domain = AF_INET;
			self->clients.maxConnections = maxConnections;
			self->clients.mu = mu;
			setSocketNonBlocking(fd);
			return self;
		} else { free(self); }
	}

exit_error:
	Mutex_destroy(mu);
	close(fd);
	return NULL;
}


ClientSocket TcpClientSocket_createAndBind(const char *ip, uint16_t port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return NULL;

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
		if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
			goto exit_error;
		}
	}

	// RST instead FIN
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(struct linger)) < 0) goto exit_error;

	self = (ClientSocket)calloc(1, sizeof(struct sClientSocket));
	if (self) {
		self->fd = fd;
		self->domain = AF_INET;
		self->inreset = true;
		self->server = NULL;
		self->idx = -1;
		setSocketNonBlocking(fd);
		return self;
	}

exit_error:
	close(fd);
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
	setsockopt(self->fd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);
	optval = idleTime;
	setsockopt(self->fd, IPPROTO_TCP, TCP_KEEPIDLE, &optval, optlen);
	optval = interval;
	setsockopt(self->fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, optlen);
	optval = count;
	setsockopt(self->fd, IPPROTO_TCP, TCP_KEEPCNT, &optval, optlen);
}

void TcpClientSocket_activateTcpNoDelay(ClientSocket self)
{
	if (self == NULL) return;
	int optval = 1;
	socklen_t optlen = sizeof(optval);
	setsockopt(self->fd, IPPROTO_TCP, TCP_NODELAY, &optval, optlen);
}

void TcpClientSocket_setUnacknowledgedTimeout(ClientSocket self, int timeoutInMs)
{
	if (self == NULL) return;
	setsockopt(self->fd, SOL_TCP, TCP_USER_TIMEOUT, &timeoutInMs, sizeof(timeoutInMs));
}


ServerSocket LocalServerSocket_create(int maxConnections, const char *address)
{
	if (address == NULL) return NULL;

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) return NULL;

	unlink(address);

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, address);
	Mutex mu = NULL;
	ServerSocket self;

	if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
		goto exit_error;
	}

	mu = Mutex_create();
	if (!mu) {
		goto exit_error;
	}

	self = (ServerSocket)calloc(1, sizeof(struct sServerSocket));
	if (self) {
		self->clients.self = (struct sClientSocket *)calloc(maxConnections, sizeof(struct sClientSocket));
		if (self->clients.self) {
			self->fd = fd;
			self->domain = AF_UNIX;
			self->clients.maxConnections = maxConnections;
			self->clients.mu = mu;
			setSocketNonBlocking(fd);
			return self;
		} else { free(self); }
	}

exit_error:
	Mutex_destroy(mu);
	close(fd);
	return NULL;
}


ClientSocket LocalClientSocket_create(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) return NULL;

	ClientSocket self = (ClientSocket)calloc(1, sizeof(struct sClientSocket));
	if (self) {
		self->fd = fd;
		self->domain = AF_UNIX;
		self->inreset = true;
		self->server = NULL;
		self->idx = -1;
		setSocketNonBlocking(fd);
		return self;
	}

// exit_error:
	close(fd);
	return NULL;
}


void ServerSocket_listen(ServerSocket self, int pending)
{
	if (self == NULL) return;
	listen(self->fd, pending);
}

ClientSocket ServerSocket_accept(ServerSocket self)
{
	if (self == NULL) return NULL;

	int fd;

	ClientSocket conSocket = NULL;
	fd = accept(self->fd, NULL, NULL);

	if (fd >= 0) {
		struct sockaddr_storage addr;
		socklen_t addrLen = sizeof(addr);
		if (getsockname(fd, (struct sockaddr*)&addr, &addrLen) != 0) goto exit_error;
		Mutex_lock(self->clients.mu);
		{
			if (self->clients.size >= self->clients.maxConnections) {
				Mutex_unlock(self->clients.mu);
				goto exit_error;
			}
			for (int i = 0; i < self->clients.maxConnections; ++i) {
				if ( self->clients.self[i].server == NULL ) {
					conSocket = &(self->clients.self[i]);
					conSocket->fd = fd;
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
	close(fd);
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

static inline void closeAndShutdownSocket(int fd)
{
	if (fd != -1) {
		shutdown(fd, SHUT_RDWR);
		close(fd);
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
	closeAndShutdownSocket(self->fd);
	self->fd = -1;
	free(self);
	return true;
}

unidesc ServerSocket_getDescriptor(ServerSocket self)
{
	if (self) {
		unidesc ret;
		ret.i32 = self->fd;
		return ret;
	}
	return Hal_getInvalidUnidesc();
}


bool ClientSocket_connectAsync(ClientSocket self, const ClientSocketAddress address)
{
	if (self == NULL || address == NULL) return false;
	if (self->server) return false;

	switch (self->domain) {
		case AF_INET: {
			struct sockaddr_in serverAddress;
			if (!prepareSocketAddress(address->ip, address->port, &serverAddress))
				return false;
			if (connect(self->fd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
				if (errno != EINPROGRESS) {
					return false;
				}
			}
		} break;
		case AF_UNIX: {
			struct sockaddr_un addr;
			addr.sun_family = AF_UNIX;
			strcpy(addr.sun_path, address->address);
			if (connect(self->fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
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
	FD_SET(self->fd, &fdSet);

	if (select(self->fd + 1, NULL, &fdSet , NULL, &timeout) == 1) {
		int so_error;
		socklen_t len = sizeof so_error;
		if (getsockopt(self->fd, SOL_SOCKET, SO_ERROR, &so_error, &len) >= 0) {
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
	if (self->fd >= 0) {
		closeAndShutdownSocket(self->fd);
		self->fd = -1;
		self->inreset = true;
	}
}

bool ClientSocket_reset(ClientSocket self)
{
	if (self == NULL) return false;
	if (self->fd >= 0 && self->server == NULL) {
		closeAndShutdownSocket(self->fd);
		self->fd = socket(self->domain, SOCK_STREAM, 0);
		self->inreset = true;
		if (self->fd >= 0) {
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

	bzero(&timeout, sizeof(struct timeval));

	FD_ZERO(&fdSet);
	FD_SET(self->fd, &fdSet);

	int selectVal = select(self->fd+1, NULL, &fdSet , NULL, &timeout);

	if (selectVal == 1) {
		/* Check if connection is established */
		int so_error;
		socklen_t len = sizeof(so_error);
		if (getsockopt(self->fd, SOL_SOCKET, SO_ERROR, &so_error, &len) >= 0) {
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

	if (self->fd == -1)
		return -1;

	int read_bytes = recv(self->fd, buf, size, MSG_DONTWAIT); // todo

	if (read_bytes == 0)
		return 0;

	if (read_bytes < 0) {
		int error = errno;
		switch (error) {
			case EAGAIN: return 0;
			default: return -1;
		}
	}

	return read_bytes;
}

int ClientSocket_write(ClientSocket self, const uint8_t *buf, int size)
{
	if (self == NULL || buf == NULL) return -1;

	if (self->fd == -1)
		return -1;

	/* MSG_NOSIGNAL - prevent send to signal SIGPIPE when peer unexpectedly closed the socket */
	int retVal = send(self->fd, buf, size, MSG_NOSIGNAL); // todo

	if (retVal <= 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
		case AF_UNIX: {
			struct sockaddr_un *paddr = (struct sockaddr_un*)addr;
			strncpy(address->address, paddr->sun_path, sizeof(address->address)-1);
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
	if (getpeername(self->fd, (struct sockaddr*) &addr, &addrLen) == 0) {
		return convertAddressToStr(&addr, address);
	}
	return false;
}

bool ClientSocket_getLocalAddress(ClientSocket self, ClientSocketAddress address)
{
	if (self == NULL || address == NULL) return false;
	struct sockaddr_storage addr;
	socklen_t addrLen = sizeof(addr);
	if (getsockname(self->fd, (struct sockaddr*) &addr, &addrLen) == 0) {
		return convertAddressToStr(&addr, address);
	}
	return false;
}

int ClientSocket_readAvailable(ClientSocket self)
{
	if (self == NULL) return -1;
	return getSocketAvailableToRead(self->fd);
}

void ClientSocket_destroy(ClientSocket self)
{
	if (self == NULL) return;
	closeAndShutdownSocket(self->fd);
	self->fd = -1;
	if (self->server) {
		ServerSocket_deleteClient(self->server, self);
	} else {
		free(self);
	}
}

unidesc ClientSocket_getDescriptor(ClientSocket self)
{
	if (self) {
		unidesc ret;
		ret.i32 = self->fd;
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

#endif // __linux__