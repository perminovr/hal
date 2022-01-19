#include "hal_base.h"

#ifndef HAL_NOT_EMPTY

#include "hal_socket_stream.h"


struct sClientSocket {
	ServerSocket server;
	int idx;
	ServerClient list;
};

struct sClients {
	int size;
	int maxConnections;
	struct sClientSocket *self;
};

struct sServerSocket {
	struct sClients clients;
};


ServerSocket TcpServerSocket_create(int maxConnections, const char *address, uint16_t port)
{
	return NULL;
}


ClientSocket TcpClientSocket_createAndBind(const char *ip, uint16_t port)
{
	return NULL;
}

ClientSocket TcpClientSocket_create()
{
	return TcpClientSocket_createAndBind(0, 0);
}

void TcpClientSocket_activateTcpKeepAlive(ClientSocket self, int idleTime, int interval, int count)
{
}

void TcpClientSocket_activateTcpNoDelay(ClientSocket self)
{
}

void TcpClientSocket_setUnacknowledgedTimeout(ClientSocket self, int timeoutInMs)
{
}


ServerSocket LocalServerSocket_create(int maxConnections, const char *address)
{
	return NULL;
}


ClientSocket LocalClientSocket_create(void)
{
	return NULL;
}


void ServerSocket_listen(ServerSocket self, int pending)
{
}

ClientSocket ServerSocket_accept(ServerSocket self)
{
	return NULL;
}

int ServerSocket_getClientsNumber(ServerSocket self)
{
	return 0;
}

ServerClient ServerSocket_getClients(ServerSocket self)
{
	return NULL;
}

bool ServerSocket_destroy(ServerSocket self)
{
	return false;
}

unidesc ServerSocket_getDescriptor(ServerSocket self)
{
	unidesc ret;
	ret.i32 = 0;
	return ret;
}


bool ClientSocket_connectAsync(ClientSocket self, const ClientSocketAddress address)
{
	return false;
}

bool ClientSocket_connect(ClientSocket self, const ClientSocketAddress address, uint32_t timeoutInMs)
{
	return false;
}

bool ClientSocket_reset(ClientSocket self)
{
	return false;
}

ClientSocketState ClientSocket_checkConnectState(ClientSocket self)
{
	return SOCKET_STATE_ERROR_UNKNOWN;
}

int ClientSocket_read(ClientSocket self, uint8_t *buf, int size)
{
	return -1;
}

int ClientSocket_write(ClientSocket self, const uint8_t *buf, int size)
{
	return -1;
}

bool ClientSocket_getPeerAddress(ClientSocket self, ClientSocketAddress address)
{
	return false;
}

bool ClientSocket_getLocalAddress(ClientSocket self, ClientSocketAddress address)
{
	return false;
}

void ClientSocket_destroy(ClientSocket self)
{
}

unidesc ClientSocket_getDescriptor(ClientSocket self)
{
	unidesc ret;
	ret.i32 = 0;
	return ret;
}

#endif // HAL_NOT_EMPTY