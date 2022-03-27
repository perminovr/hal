
#if defined(_WIN32) || defined(_WIN64)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hal_serial.h"
#include "hal_syshelper.h"
#include <winsock2.h>
#include <windows.h>


typedef enum {
	CREATED, INITED, OPENED
} PortState;


struct sSerialPort {
	char interfaceName[32];
	HANDLE h;
	HalShFdescHelper fdh;
	int baudRate;
	uint8_t dataBits;
	char parity;
	uint8_t stopBits;
	int timeout;
	SerialPortError lastError;
	PortState state;
};


SerialPort SerialPort_create(const char *interfaceName)
{
	if (interfaceName == NULL) return NULL;
	SerialPort self = (SerialPort)calloc(1, sizeof(struct sSerialPort));
	if (self != NULL) {
		self->state = CREATED;
		self->fdh = NULL;
		self->h = INVALID_HANDLE_VALUE;
		self->baudRate = 9600;
		self->dataBits = 8;
		self->stopBits = 1;
		self->parity = 'N';
		self->timeout = 100;
		strncpy(self->interfaceName, interfaceName, sizeof(self->interfaceName)-1);
		self->lastError = SERIAL_PORT_ERROR_NONE;
	}
	return self;
}

static void SerialPort_checkAndClose(SerialPort self)
{
	if (self->state > INITED) {
		HalShFdescHelper_destroy(self->fdh);
		self->fdh = NULL;
		CloseHandle(self->h);
		self->h = INVALID_HANDLE_VALUE;
		self->state = INITED;
	}
}

bool SerialPort_reinit(SerialPort self, int baudRate, uint8_t dataBits, char parity, uint8_t stopBits)
{
	if (self == NULL) return false;
	SerialPort_checkAndClose(self);
	self->state = INITED;
	self->baudRate = baudRate;
	self->dataBits = dataBits;
	self->stopBits = stopBits;
	self->parity = parity;
	self->lastError = SERIAL_PORT_ERROR_NONE;
	return true;
}

void SerialPort_destroy(SerialPort self)
{
	if (self == NULL) return;
	SerialPort_checkAndClose(self);
	free(self);
}

bool SerialPort_open(SerialPort self)
{
	if (self == NULL) return false;

	self->lastError = SERIAL_PORT_ERROR_NONE;

	self->h = CreateFileA((LPCSTR)self->interfaceName, GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, 0, NULL);

	if (self->h == INVALID_HANDLE_VALUE) {
		self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
		return false;
	}

	DWORD baudrate;
	COMMTIMEOUTS timeouts = { 0 };
	DCB serialParams = { 0 };
	serialParams.DCBlength = sizeof(DCB);

	if (GetCommState(self->h, &serialParams) == FALSE) {
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		goto exit_error;
	}

	switch (self->baudRate) {
		case 110: baudrate = CBR_110; break;
		case 300: baudrate = CBR_300; break;
		case 600: baudrate = CBR_600; break;
		case 1200: baudrate = CBR_1200; break;
		case 2400: baudrate = CBR_2400; break;
		case 4800: baudrate = CBR_4800; break;
		case 9600: baudrate = CBR_9600; break;
		case 19200: baudrate = CBR_19200; break;
		case 38400:  baudrate = CBR_38400; break;
		case 57600: baudrate = CBR_57600; break;
		case 115200: baudrate = CBR_115200; break;
		default:
			baudrate = CBR_9600;
			self->lastError = SERIAL_PORT_ERROR_INVALID_BAUDRATE;
			break;
    }
	serialParams.BaudRate = baudrate;

	/* Set data bits (5/6/7/8) */
	serialParams.ByteSize = self->dataBits;

	/* Set stop bits (1/2) */
	if (self->stopBits == 1)
		serialParams.StopBits = ONESTOPBIT;
	else /* 2 */
		serialParams.StopBits = TWOSTOPBITS;

	if (self->parity == 'N')
		serialParams.Parity = NOPARITY;
	else if (self->parity == 'E')
		serialParams.Parity = EVENPARITY;
	else /* 'O' */
		serialParams.Parity = ODDPARITY;
		
	if (SetCommState(self->h, &serialParams) == FALSE) {
		self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;
		goto exit_error;
	}

	timeouts.ReadIntervalTimeout = 0; // max char spacing
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0; // max frame spacing
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;
	if (SetCommTimeouts(self->h, &timeouts) == FALSE) {
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		goto exit_error;
	}

	if (SetCommMask(self->h, EV_RXCHAR) == FALSE) {
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		goto exit_error;
	}
	
	unidesc ud; ud.u64 = (uint64_t)self->h;
	int charsSpacing = (int)((8000.0f / (float)self->baudRate) * 5); // 5 symbols
	charsSpacing += (charsSpacing)? 0 : 1;
	self->fdh = HalShFdescHelper_create(ud, 256, charsSpacing);
	if (!self->fdh) {
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		goto exit_error;
	}

	self->state = OPENED;
	return true;

exit_error:
	CloseHandle(self->h);
	self->h = INVALID_HANDLE_VALUE;
	return false;
}

void SerialPort_close(SerialPort self)
{
	if (self == NULL) return;
	SerialPort_checkAndClose(self);
}

static int SerialPort_readByteTimeout(SerialPort self, SOCKET s, struct timeval *timeout)
{
	int rc;
	fd_set set;
	uint8_t buf[1];

	self->lastError = SERIAL_PORT_ERROR_NONE;

	FD_ZERO(&set);
	FD_SET(s, &set);

	int ret = select(0, &set, NULL, NULL, timeout);

    if (ret == -1) {
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		return -1;
    } else if (ret == 0) {
		return -1;
	} else {
        rc = HalShFdescHelper_read(self->fdh, buf, 1);
		if (rc > 0) return (int)buf[0];
		return -1;
	}
}

int SerialPort_readByte(SerialPort self)
{
	if (self == NULL) return -1;
	SOCKET s = (SOCKET)(SerialPort_getDescriptor(self).u64);
	struct timeval tv;
	tv.tv_sec = self->timeout/1000;
	tv.tv_usec = (self->timeout % 1000) * 1000;
	return SerialPort_readByteTimeout(self, s, &tv);
}

int SerialPort_read(SerialPort self, uint8_t *buffer, int bufSize)
{
	if (self == NULL || buffer == NULL) return -1;

	int i;
	int res;

	SOCKET s = (SOCKET)(SerialPort_getDescriptor(self).u64);
	struct timeval tv;
	tv.tv_sec = self->timeout/1000;
	tv.tv_usec = (self->timeout % 1000) * 1000;

	for (i = 0; i < bufSize; ++i) {
		res = SerialPort_readByteTimeout(self, s, &tv);
		if (res >= 0) {
			buffer[i] = (uint8_t)res;
		} else {
			break;
		}
	}

	if (self->lastError != SERIAL_PORT_ERROR_NONE) return -1;
	return i;
}

int SerialPort_write(SerialPort self, uint8_t *buffer, int bufSize)
{
	if (self == NULL || buffer == NULL) return -1;
	self->lastError = SERIAL_PORT_ERROR_NONE;
	return HalShFdescHelper_write(self->fdh, buffer, bufSize);
}

unidesc SerialPort_getDescriptor(SerialPort self)
{
	if (self) {
		if (self->fdh) {
			return HalShFdesc_getDescriptor(HalShFdescHelper_getDescriptor(self->fdh));
		}
	}
	return Hal_getInvalidUnidesc();
}

int SerialPort_getBaudRate(SerialPort self)
{
	if (self == NULL) return -1;
	return self->baudRate;
}

void SerialPort_discardInBuffer(SerialPort self)
{
	if (self == NULL) return;
	if (self->h == INVALID_HANDLE_VALUE) return;
	PurgeComm(self->h, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

void SerialPort_setTimeout(SerialPort self, int timeout)
{
	if (self == NULL) return;
	self->timeout = timeout;
}

SerialPortError SerialPort_getLastError(SerialPort self)
{
	if (self == NULL) return SERIAL_PORT_ERROR_UNKNOWN;
	return self->lastError;
}

#endif // _WIN32 || _WIN64
