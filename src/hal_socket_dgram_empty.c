#include "hal_base.h"

#ifndef HAL_NOT_EMPTY

#include "hal_socket_dgram.h"


struct sDgramSocket {
	union uDgramSocketAddress remote;
};


DgramSocket UdpDgramSocket_createAndBind(const char *ip, uint16_t port)
{
	return NULL;
}

DgramSocket UdpDgramSocket_create(void)
{
	return UdpDgramSocket_createAndBind(0, 0);
}

bool UdpDgramSocket_bind(DgramSocket self, const char *ip, uint16_t port)
{
	return false;
}


DgramSocket LocalDgramSocket_create(const char *address)
{
	return NULL;
}


DgramSocket EtherDgramSocket_create(const char *interface, uint16_t ethTypeFilter)
{
	return NULL;
}

int EtherDgramSocket_setHeader(uint8_t *header, const uint8_t *src, const uint8_t *dst, uint16_t ethType)
{
	return -1;
}

void EtherDgramSocket_getHeader(const uint8_t *header, uint8_t *src, uint8_t *dst, uint16_t *ethType)
{
}

bool EtherDgramSocket_getInterfaceMACAddress(const char *interface, uint8_t *addr)
{
	return false;
}

uint8_t *EtherDgramSocket_getSocketMACAddress(DgramSocket self, uint8_t *addr)
{
	return NULL;
}


bool DgramSocket_reset(DgramSocket self)
{
	return false;
}

void DgramSocket_setRemote(DgramSocket self, const DgramSocketAddress addr)
{
}

int DgramSocket_readFrom(DgramSocket self, DgramSocketAddress addr, uint8_t *buf, int size)
{
	return -1;
}

int DgramSocket_writeTo(DgramSocket self, const DgramSocketAddress addr, const uint8_t *buf, int size)
{
	return -1;
}

int DgramSocket_read(DgramSocket self, uint8_t *buf, int size)
{
	return -1;
}

int DgramSocket_write(DgramSocket self, const uint8_t *buf, int size)
{
	return -1;
}

void DgramSocket_destroy(DgramSocket self)
{
}

unidesc DgramSocket_getDescriptor(DgramSocket self)
{
	unidesc ret;
	ret.i32 = 0;
	return ret;
}


#endif // HAL_NOT_EMPTY