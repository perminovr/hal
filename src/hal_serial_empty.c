#include "hal_base.h"

#ifndef HAL_NOT_EMPTY

#include "hal_serial.h"


typedef enum {
	CREATED, INITED, OPENED
} PortState;


struct sSerialPort {
	char interfaceName[32];
	SerialPortError lastError;
	PortState state;
};


SerialPort SerialPort_create(const char *interfaceName)
{
	return NULL;
}

bool SerialPort_reinit(SerialPort self, int baudRate, uint8_t dataBits, char parity, uint8_t stopBits)
{
	return false;
}

void SerialPort_destroy(SerialPort self)
{
}

bool SerialPort_open(SerialPort self)
{
	return false;
}

void SerialPort_close(SerialPort self)
{
}

int SerialPort_readByte(SerialPort self)
{
	return -1;
}

int SerialPort_read(SerialPort self, uint8_t *buffer, int bufSize)
{
	return -1;
}

int SerialPort_write(SerialPort self, uint8_t *buffer, int bufSize)
{
	return -1;
}

unidesc SerialPort_getDescriptor(SerialPort self)
{
	unidesc ret;
	ret.i32 = 0;
	return ret;
}

int SerialPort_getBaudRate(SerialPort self)
{
	return -1;
}

void SerialPort_discardInBuffer(SerialPort self)
{
}

void SerialPort_setTimeout(SerialPort self, int timeout)
{
}

SerialPortError SerialPort_getLastError(SerialPort self)
{
	return SERIAL_PORT_ERROR_UNKNOWN;
}

#endif // HAL_NOT_EMPTY
