
#ifdef __linux__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <sys/select.h>
#include <unistd.h>

#include "hal_serial.h"


typedef enum {
	CREATED, INITED, OPENED
} PortState;


struct sSerialPort {
	char interfaceName[32];
	int fd;
	int baudRate;
	uint8_t dataBits;
	char parity;
	uint8_t stopBits;
	struct timeval timeout;
	SerialPortError lastError;
	PortState state;
};


SerialPort SerialPort_create(const char *interfaceName)
{
	if (interfaceName == NULL) return NULL;
	SerialPort self = (SerialPort)calloc(1, sizeof(struct sSerialPort));
	if (self != NULL) {
		self->state = CREATED;
		self->fd = -1;
		self->baudRate = 9600;
		self->dataBits = 8;
		self->stopBits = 1;
		self->parity = 'N';
		self->timeout.tv_sec = 0;
		self->timeout.tv_usec = 100*1000;
		strncpy(self->interfaceName, interfaceName, sizeof(self->interfaceName)-1);
		self->lastError = SERIAL_PORT_ERROR_NONE;
	}
	return self;
}

static void SerialPort_checkAndClose(SerialPort self)
{
	if (self->state > INITED) {
		close(self->fd);
		self->fd = -1;
		self->state = INITED;
	}
}

bool SerialPort_reinit(SerialPort self, int baudRate, uint8_t dataBits, char parity, uint8_t stopBits)
{
	if (self == NULL) return false;
	SerialPort_checkAndClose(self);
	self->state = INITED;
	self->fd = -1;
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

	self->fd = open(self->interfaceName, O_RDWR | O_NOCTTY | O_NDELAY | O_EXCL);
	if (self->fd < 0) {
		self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
		return false;
	}

	struct termios tios;
	speed_t baudrate;

	tcgetattr(self->fd, &tios);

	tios.c_lflag = 0;
	tios.c_iflag = 0;
	tios.c_oflag = 0;
	tios.c_cc[VMIN] = 0;
	tios.c_cc[VTIME] = 0;

	tios.c_cflag = (CREAD | CLOCAL);

	switch (self->baudRate) {
		#ifdef B110
		case 110: baudrate = B110; break;
		#endif
		#ifdef B300
		case 300: baudrate = B300; break;
		#endif
		#ifdef B600
		case 600: baudrate = B600; break;
		#endif
		#ifdef B1200
		case 1200: baudrate = B1200; break;
		#endif
		#ifdef B2400
		case 2400: baudrate = B2400; break;
		#endif
		#ifdef B4800
		case 4800: baudrate = B4800; break;
		#endif
		#ifdef B9600
		case 9600: baudrate = B9600; break;
		#endif
		#ifdef B19200
		case 19200: baudrate = B19200; break;
		#endif
		#ifdef B38400
		case 38400:  baudrate = B38400; break;
		#endif
		#ifdef B57600
		case 57600: baudrate = B57600; break;
		#endif
		#ifdef B115200
		case 115200: baudrate = B115200; break;
		#endif
		#ifdef B230400
		case 230400: baudrate = B230400; break;
		#endif
		#ifdef B460800
		case 460800: baudrate = B460800; break;
		#endif
		#ifdef B500000
		case 500000: baudrate = B500000; break;
		#endif
		#ifdef B921600
		case 921600: baudrate = B921600; break;
		#endif
		#ifdef B1000000
		case 1000000: baudrate = B1000000; break;
		#endif
		#ifdef B1152000
		case 1152000: baudrate = B1152000; break;
		#endif
		#ifdef B1500000
		case 1500000: baudrate = B1500000; break;
		#endif
		#ifdef B2000000
		case 2000000: baudrate = B2000000; break;
		#endif
		#ifdef B2500000
		case 2500000: baudrate = B2500000; break;
		#endif
		#ifdef B3000000
		case 3000000: baudrate = B3000000; break;
		#endif
		#ifdef B3500000
		case 3500000: baudrate = B3500000; break;
		#endif
		#ifdef B4000000
		case 4000000: baudrate = B4000000; break;
		#endif
		default:
			baudrate = B9600;
			self->lastError = SERIAL_PORT_ERROR_INVALID_BAUDRATE;
			break;
	}

	/* Set baud rate */
	if ((cfsetispeed(&tios, baudrate) < 0) || (cfsetospeed(&tios, baudrate) < 0)) {
		self->lastError = SERIAL_PORT_ERROR_INVALID_BAUDRATE;
		goto exit_error;
	}

	/* Set data bits (5/6/7/8) */
	switch (self->dataBits) {
	case 5: tios.c_cflag |= CS5; break;
	case 6: tios.c_cflag |= CS6; break;
	case 7: tios.c_cflag |= CS7; break;
	default: tios.c_cflag |= CS8; break;
	}

	/* Set stop bits (1/2) */
	if (self->stopBits != 1) {
		tios.c_cflag |= CSTOPB;
	}

	if (self->parity != 'N') {
		tios.c_iflag |= INPCK;
		if (self->parity == 'E') {
			tios.c_cflag |= PARENB;
			tios.c_cflag &=~ PARODD;
		} else { /* 'O' */
			tios.c_cflag |= PARENB;
			tios.c_cflag |= PARODD;
		}
	}

	tios.c_iflag |= IGNBRK; /* Set ignore break to allow 0xff characters */
	tios.c_iflag |= IGNPAR;

	if (tcsetattr(self->fd, TCSANOW, &tios) < 0) {
		self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;
		goto exit_error;
	}

	self->state = OPENED;
	return true;

exit_error:
	close(self->fd);
	self->fd = -1;
	return false;
}

void SerialPort_close(SerialPort self)
{
	if (self == NULL) return;
	SerialPort_checkAndClose(self);
}

static int SerialPort_readByteTimeout(SerialPort self, struct timeval *timeout)
{
	int rc;
	fd_set set;
	uint8_t buf[1];

	self->lastError = SERIAL_PORT_ERROR_NONE;

	FD_ZERO(&set);
	FD_SET(self->fd, &set);

	int ret = select(self->fd+1, &set, NULL, NULL, timeout);

	if (ret == -1) {
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		return -1;
	} else if (ret == 0) {
		return -1;
	} else {
		rc = read(self->fd, (char*) buf, 1);
		if (rc > 0) return (int) buf[0];
		return -1;
	}
}

int SerialPort_readByte(SerialPort self)
{
	if (self == NULL) return -1;
	if (self->fd == -1) return -1;
	struct timeval timeout = self->timeout;
	return SerialPort_readByteTimeout(self, &timeout);
}

int SerialPort_read(SerialPort self, uint8_t *buffer, int bufSize)
{
	if (self == NULL || buffer == NULL) return -1;
	if (self->fd == -1) return -1;

	int i;
	int res;
	struct timeval timeout = self->timeout;

	for (i = 0; i < bufSize; ++i) {
		res = SerialPort_readByteTimeout(self, &timeout);
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
	if (self->fd == -1) return -1;
	self->lastError = SERIAL_PORT_ERROR_NONE;
	ssize_t result = write(self->fd, buffer, bufSize);
	return result;
}

int SerialPort_writeAndWait(SerialPort self, uint8_t *buffer, int bufSize)
{
	ssize_t result = SerialPort_write(self, buffer, bufSize);
	if (result > 0) { tcdrain(self->fd); }
	return result;
}

unidesc SerialPort_getDescriptor(SerialPort self)
{
	if (self) {
		unidesc ret;
		ret.i32 = self->fd;
		return ret;
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
	if (self->fd == -1) return;
	tcflush(self->fd, TCIOFLUSH);
}

void SerialPort_setTimeout(SerialPort self, int timeout)
{
	if (self == NULL) return;
	self->timeout.tv_sec = timeout / 1000;
	self->timeout.tv_usec = (timeout % 1000) * 1000;
}

SerialPortError SerialPort_getLastError(SerialPort self)
{
	if (self == NULL) return SERIAL_PORT_ERROR_UNKNOWN;
	return self->lastError;
}

#endif // __linux__
