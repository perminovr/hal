
#include <stdio.h>
#include "hal_serial.h"
#include "hal_poll.h"
#include "hal_time.h"
#include "hal_thread.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
// windows: com0com
// linux: socat PTY,raw,echo=0,link=/tmp/ttyTEST1 PTY,raw,echo=0,link=/tmp/ttyTEST2
#if defined (_WIN32) || defined (_WIN64)
	const char *com1 = "\\\\.\\COM3";
	const char *com2 = "\\\\.\\COM4";
#endif
#if defined(__linux__)
	const char *com1 = "/tmp/ttyTEST1";
	const char *com2 = "/tmp/ttyTEST2";
#endif
	int rc, revents;
	uint64_t ts, ts0;
	SerialPort s1 = SerialPort_create(com1);
	SerialPort s2 = SerialPort_create(com2);
	if (SerialPort_reinit(s1, 115200, 8, 'N', 1) == false) { err(); return 1; }
	if (SerialPort_reinit(s2, 115200, 8, 'N', 1) == false) { err(); return 1; }
	SerialPort_setTimeout(s1, 100);
	SerialPort_setTimeout(s2, 100);
	if (SerialPort_open(s1) == false) { err(); return 1; }
	if (SerialPort_open(s2) == false) { err(); return 1; }
	if (SerialPort_getLastError(s1) != SERIAL_PORT_ERROR_NONE) { err(); return 1; }
	if (SerialPort_getLastError(s2) != SERIAL_PORT_ERROR_NONE) { err(); return 1; }
	SerialPort_discardInBuffer(s1);
	SerialPort_discardInBuffer(s2);
	char buf[65535*5];
	for (int i = 0; i < 257; ++i) {
		buf[i] = (char)i;
	}
	switch (test) {
		case 1: { // base
			// write
			rc = SerialPort_write(s1, buf, 257);
			if (rc != 257) { err(); return 1; }
			memset(buf, 0, 257);
			ts0 = Hal_getTimeInMs();
			// read less than max
			rc = SerialPort_read(s2, buf, 255);
			ts = Hal_getTimeInMs() - ts0;
			if (rc != 255) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// read one
			ts0 = Hal_getTimeInMs();
			rc = SerialPort_readByte(s2);
			ts = Hal_getTimeInMs() - ts0;
			if (rc < 0) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			buf[255] = (char)rc;
			// read Nmax and Nmax+1 byte (timeout)
			ts0 = Hal_getTimeInMs();
			rc = SerialPort_read(s2, buf+256, 2);
			ts = Hal_getTimeInMs() - ts0;
			if (rc != 1) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			// read another one (timeout)
			ts0 = Hal_getTimeInMs();
			rc = SerialPort_readByte(s2);
			ts = Hal_getTimeInMs() - ts0;
			if (rc >= 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			// compare
			for (int i = 0; i < 257; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// close
			SerialPort_close(s2);
			ts0 = Hal_getTimeInMs();
			rc = SerialPort_readByte(s2);
			ts = Hal_getTimeInMs() - ts0;
			if (rc >= 0) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// clean
			SerialPort_destroy(s1);
			SerialPort_destroy(s2);
			return 0;
		} break;
		case 2: { // discard buffer
			// write
			rc = SerialPort_write(s1, buf, 257);
			if (rc != 257) { err(); return 1; }
			Thread_sleep(10); // wait for bytes
			SerialPort_discardInBuffer(s2);
			// read (timeout)
			ts0 = Hal_getTimeInMs();
			rc = SerialPort_readByte(s2);
			ts = Hal_getTimeInMs() - ts0;
			if (rc >= 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			// clean
			SerialPort_destroy(s1);
			SerialPort_destroy(s2);
			return 0;
		} break;
		case 3: { // reinit
			if (SerialPort_reinit(s1, 9600, 8, 'N', 1) == false) { err(); return 1; }
			if (SerialPort_reinit(s2, 9600, 8, 'N', 1) == false) { err(); return 1; }
			if (SerialPort_open(s1) == false) { err(); return 1; }
			if (SerialPort_open(s2) == false) { err(); return 1; }
			if (SerialPort_getLastError(s1) != SERIAL_PORT_ERROR_NONE) { err(); return 1; }
			if (SerialPort_getLastError(s2) != SERIAL_PORT_ERROR_NONE) { err(); return 1; }
			// write
			rc = SerialPort_write(s1, buf, 257);
			if (rc != 257) { err(); return 1; }
			memset(buf, 0, 257);
			ts0 = Hal_getTimeInMs();
			// read
			rc = SerialPort_read(s2, buf, 257);
			ts = Hal_getTimeInMs() - ts0;
			if (rc != 257) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// compare
			for (int i = 0; i < 257; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// clean
			SerialPort_destroy(s1);
			SerialPort_destroy(s2);
			return 0;
		} break;
		case 4: { // desc
			// pollout1
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(SerialPort_getDescriptor(s1), HAL_POLLOUT, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLOUT) == 0 ) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// pollin1
			SerialPort_discardInBuffer(s2);
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(SerialPort_getDescriptor(s2), HAL_POLLIN, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc != 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) != 0 ) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			// write
			rc = SerialPort_write(s1, buf, 257);
			if (rc != 257) { err(); return 1; }
			memset(buf, 0, 257);
			ts0 = Hal_getTimeInMs();
			// pollin2
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(SerialPort_getDescriptor(s2), HAL_POLLIN, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) == 0 ) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// read
			ts0 = Hal_getTimeInMs();
			rc = SerialPort_read(s2, buf, 257);
			ts = Hal_getTimeInMs() - ts0;
			if (rc != 257) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// compare
			for (int i = 0; i < 257; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// clean
			SerialPort_destroy(s1);
			SerialPort_destroy(s2);
			return 0;
		} break;
	}

	{ err(); return 1; }
}