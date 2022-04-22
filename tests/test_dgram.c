
#include <stdio.h>
#include "hal_socket_dgram.h"
#include "hal_poll.h"
#include "hal_thread.h"
#include "hal_time.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

static void prepEthFrame(uint8_t *buf)
{
	static const uint8_t goose[] = {
		0x01, 0x0c, 0xcd, 0x01, 0x00, 0x00, 0x08, 0x00, 0x27, 0x62, 0xb8, 0xa2, 0x81, 0x00, 0x80, 0x00,
		0x88, 0xb8, 0x00, 0x02, 0x00, 0x77, 0x80, 0x00, 0x00, 0x00, 0x61, 0x6d, 0x80, 0x15, 0x49, 0x45,
		0x44, 0x31, 0x4c, 0x44, 0x32, 0x2f, 0x4c, 0x4c, 0x4e, 0x30, 0x24, 0x47, 0x4f, 0x24, 0x67, 0x63,
		0x62, 0x30, 0x32, 0x81, 0x01, 0x28, 0x82, 0x15, 0x49, 0x45, 0x44, 0x31, 0x4c, 0x44, 0x32, 0x2f,
		0x4c, 0x4c, 0x4e, 0x30, 0x24, 0x42, 0x4c, 0x4f, 0x43, 0x4b, 0x5f, 0x4c, 0x32, 0x83, 0x01, 0x32,
		0x84, 0x08, 0x68, 0x75, 0xb6, 0x46, 0xd4, 0x3f, 0xff, 0x3f, 0x85, 0x01, 0x01, 0x86, 0x01, 0x01,
		0x87, 0x01, 0x01, 0x88, 0x01, 0x01, 0x89, 0x01, 0x00, 0x8a, 0x01, 0x09, 0xab, 0x1b, 0x83, 0x01,
		0x00, 0x83, 0x01, 0x00, 0x83, 0x01, 0x00, 0x83, 0x01, 0x00, 0x83, 0x01, 0x00, 0x83, 0x01, 0x00,
		0x83, 0x01, 0x00, 0x83, 0x01, 0x00, 0x83, 0x01, 0x00
	};
	memcpy(buf, goose, sizeof(goose));
}

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
	DgramSocket s1, s2, s3;
	union uDgramSocketAddress addr;
	int rc, revents;
	char buf[65535];
	#if defined (_WIN32) || defined (_WIN64)
	const char *iface = "loopback_0";
	#endif
	#if defined(__linux__)
	const char *iface = "lo";
	#endif
	switch (test) {
		case 1: { // udp base
			// link
			s1 = UdpDgramSocket_createAndBind("127.0.0.1", 43555);
			s2 = UdpDgramSocket_create();
			if (UdpDgramSocket_bind(s2, "127.0.0.1", 43556) == false) { err(); return 1; }
			strcpy(addr.ip, "127.0.0.1");
			addr.port = 43556;
			DgramSocket_setRemote(s1, &addr);
			addr.port = 43555;
			DgramSocket_setRemote(s2, &addr);
			memset(&addr, 0, sizeof(addr));
			DgramSocket_getRemote(s1, &addr);
			if ( strcmp(addr.ip, "127.0.0.1") != 0 ) { err(); return 1; }
			if (addr.port != 43556) { err(); return 1; }
			// write
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_write(s1, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			// check available
			rc = DgramSocket_readAvailable(s2, true);
			if (rc != 60000) { err(); return 1; }
			// write next dgram
			rc = DgramSocket_writeTo(s1, &addr, buf+60000, 5000);
			if (rc != 5000) { err(); return 1; }
			// read
			memset(buf, 0, 65535);
			rc = DgramSocket_read(s2, buf, 60001);
			if (rc != 60000) { err(); return 1; }
			memset(&addr, 0, sizeof(addr));
			rc = DgramSocket_readFrom(s2, &addr, buf+60000, 5001);
			if (rc != 5000) { err(); return 1; }
			if ( strcmp(addr.ip, "127.0.0.1") != 0 ) { err(); return 1; }
			if (addr.port != 43555) { err(); return 1; }
			for (int i = 0; i < 65000; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			return 0;
		} break;
		case 2: { // udp reuse
			s1 = UdpDgramSocket_create();
			UdpDgramSocket_setReuse(s1, true);
			if (UdpDgramSocket_bind(s1, "127.0.0.1", 43555) == false) { err(); return 1; }
			s2 = UdpDgramSocket_create();
			UdpDgramSocket_setReuse(s2, true);
			if (UdpDgramSocket_bind(s2, "127.0.0.1", 43555) == false) { err(); return 1; }
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			return 0;
		} break;
		case 3: { // udp reset
			// link
			s1 = UdpDgramSocket_createAndBind("127.0.0.1", 43555);
			s2 = UdpDgramSocket_createAndBind("127.0.0.1", 43556);
			strcpy(addr.ip, "127.0.0.1");
			addr.port = 43556;
			DgramSocket_setRemote(s1, &addr);
			addr.port = 43555;
			DgramSocket_setRemote(s2, &addr);
			// wr/rd 1
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_write(s1, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			memset(buf, 0, 65535);
			rc = DgramSocket_read(s2, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			for (int i = 0; i < 60000; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// reset
			DgramSocket_reset(s1);
			// wr/rd 2
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			UdpDgramSocket_bind(s1, "127.0.0.1", 43555);
			rc = DgramSocket_write(s1, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			memset(buf, 0, 65535);
			rc = DgramSocket_read(s2, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			for (int i = 0; i < 60000; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			return 0;
		} break;
		case 4: { // udp mcast
			printf("test only works if all network adapters are disabled due to loopback (windows10+)\n");
			s1 = UdpDgramSocket_createAndBind("127.0.0.1", 43555);
			s2 = UdpDgramSocket_createAndBind(NULL, 43556);
			strcpy(addr.ip, "239.255.255.251");
			addr.port = 43556;
			if (UdpDgramSocket_joinGroup(s2, addr.ip, iface) == false) { err(); return 1; }
			// write
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_writeTo(s1, &addr, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			// read
			memset(&addr, 0, sizeof(addr));
			memset(buf, 0, 65535);
			rc = DgramSocket_readFrom(s2, &addr, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			return 0;
		} break;
		case 5: { // udp desc
			uint64_t ts, ts0;
			// link
			s1 = UdpDgramSocket_createAndBind("127.0.0.1", 43555);
			s2 = UdpDgramSocket_createAndBind("127.0.0.1", 43556);
			strcpy(addr.ip, "127.0.0.1");
			addr.port = 43556;
			DgramSocket_setRemote(s1, &addr);
			addr.port = 43555;
			DgramSocket_setRemote(s2, &addr);
			// check poll 1
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(DgramSocket_getDescriptor(s2), HAL_POLLIN, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc != 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			// write
			rc = DgramSocket_write(s1, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			// check poll 2
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(DgramSocket_getDescriptor(s2), HAL_POLLIN, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// read
			rc = DgramSocket_read(s2, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			// check poll 3
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(DgramSocket_getDescriptor(s2), HAL_POLLIN|HAL_POLLOUT, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) != 0) { err(); return 1; }
			if ( (revents&HAL_POLLOUT) == 0) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			return 0;
		} break;
		case 6: { // udp parthner filter
			// link
			s1 = UdpDgramSocket_createAndBind("127.0.0.1", 43555);
			s2 = UdpDgramSocket_createAndBind("127.0.0.1", 43556);
			s3 = UdpDgramSocket_createAndBind("127.0.0.1", 43557);
			strcpy(addr.ip, "127.0.0.1");
			addr.port = 43556;
			DgramSocket_setRemote(s1, &addr);
			DgramSocket_setRemote(s3, &addr);
			addr.port = 43555;
			DgramSocket_setRemote(s2, &addr);
			// write 1
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_write(s1, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_write(s3, buf+1000, 1000);
			if (rc != 1000) { err(); return 1; }
			// read 1
			memset(buf, 0, 65535);
			rc = DgramSocket_read(s2, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_readAvailable(s2, true);
			if (rc != 0) { err(); return 1; }
			rc = DgramSocket_read(s2, buf, 1000);
			if (rc != 0) { err(); return 1; }
			for (int i = 0; i < 1000; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// write 2
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_write(s3, buf+1000, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_write(s1, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			// read 2
			memset(buf, 0, 65535);
			rc = DgramSocket_readAvailable(s2, true);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_read(s2, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			for (int i = 0; i < 1000; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// write 3
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_write(s3, buf+1000, 1000);
			if (rc != 1000) { err(); return 1; }
			// read 3
			rc = DgramSocket_readAvailable(s2, false);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_read(s2, buf, 1000);
			if (rc != 0) { err(); return 1; }
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			DgramSocket_destroy(s3);
			return 0;
		} break;
		case 10: { // local base
			// link
			LocalDgramSocket_unlinkAddress("/tmp/local-d-test0");
			LocalDgramSocket_unlinkAddress("/tmp/local-d-test1");
			s1 = LocalDgramSocket_create("/tmp/local-d-test0");
			s2 = LocalDgramSocket_create("/tmp/local-d-test1");
			strcpy(addr.address, "/tmp/local-d-test1");
			DgramSocket_setRemote(s1, &addr);
			strcpy(addr.address, "/tmp/local-d-test0");
			DgramSocket_setRemote(s2, &addr);
			memset(&addr, 0, sizeof(addr));
			DgramSocket_getRemote(s1, &addr);
			if (strlen(addr.address) == 0) { err(); return 1; } // diff names depend on system, just check for 0 len
			// write
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_write(s1, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			// check available
			rc = DgramSocket_readAvailable(s2, true);
			if (rc != 60000) { err(); return 1; }
			// write next dgram
			rc = DgramSocket_writeTo(s1, &addr, buf+60000, 5000);
			if (rc != 5000) { err(); return 1; }
			// read
			memset(buf, 0, 65535);
			rc = DgramSocket_read(s2, buf, 60001);
			if (rc != 60000) { err(); return 1; }
			memset(&addr, 0, sizeof(addr));
			rc = DgramSocket_readFrom(s2, &addr, buf+60000, 5001);
			if (rc != 5000) { err(); return 1; }
			if (strlen(addr.address) == 0) { err(); return 1; }
			for (int i = 0; i < 65000; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			return 0;
		} break;
		case 11: { // local reset
			// link
			LocalDgramSocket_unlinkAddress("/tmp/local-d-test0");
			s1 = LocalDgramSocket_create("/tmp/local-d-test0");
			// reset
			if (DgramSocket_reset(s1) != false) { err(); return 1; }
			// clean
			DgramSocket_destroy(s1);
			return 0;
		} break;
		case 12: { // local desc
			uint64_t ts, ts0;
			// link
			LocalDgramSocket_unlinkAddress("/tmp/local-d-test0");
			LocalDgramSocket_unlinkAddress("/tmp/local-d-test1");
			s1 = LocalDgramSocket_create("/tmp/local-d-test0");
			s2 = LocalDgramSocket_create("/tmp/local-d-test1");
			strcpy(addr.address, "/tmp/local-d-test1");
			DgramSocket_setRemote(s1, &addr);
			strcpy(addr.address, "/tmp/local-d-test0");
			DgramSocket_setRemote(s2, &addr);
			// check poll 1
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(DgramSocket_getDescriptor(s2), HAL_POLLIN, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc != 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			// write
			rc = DgramSocket_write(s1, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			// check poll 2
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(DgramSocket_getDescriptor(s2), HAL_POLLIN, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// read
			rc = DgramSocket_read(s2, buf, 60000);
			if (rc != 60000) { err(); return 1; }
			// check poll 3
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(DgramSocket_getDescriptor(s2), HAL_POLLIN|HAL_POLLOUT, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) != 0) { err(); return 1; }
			if ( (revents&HAL_POLLOUT) == 0) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			return 0;
		} break;
		case 13: { // local parthner filter
			// link
			LocalDgramSocket_unlinkAddress("/tmp/local-d-test0");
			LocalDgramSocket_unlinkAddress("/tmp/local-d-test1");
			LocalDgramSocket_unlinkAddress("/tmp/local-d-test2");
			s1 = LocalDgramSocket_create("/tmp/local-d-test0");
			s2 = LocalDgramSocket_create("/tmp/local-d-test1");
			s3 = LocalDgramSocket_create("/tmp/local-d-test2");
			strcpy(addr.address, "/tmp/local-d-test1");
			DgramSocket_setRemote(s1, &addr);
			DgramSocket_setRemote(s3, &addr);
			strcpy(addr.address, "/tmp/local-d-test0");
			DgramSocket_setRemote(s2, &addr);
			// write 1
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_write(s1, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_write(s3, buf+1000, 1000);
			if (rc != 1000) { err(); return 1; }
			// read 1
			memset(buf, 0, 65535);
			rc = DgramSocket_read(s2, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_readAvailable(s2, true);
			if (rc != 0) { err(); return 1; }
			rc = DgramSocket_read(s2, buf, 1000);
			if (rc != 0) { err(); return 1; }
			for (int i = 0; i < 1000; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// write 2
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_write(s3, buf+1000, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_write(s1, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			// read 2
			memset(buf, 0, 65535);
			rc = DgramSocket_readAvailable(s2, true);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_read(s2, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			for (int i = 0; i < 1000; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// write 3
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = DgramSocket_write(s3, buf+1000, 1000);
			if (rc != 1000) { err(); return 1; }
			// read 3
			rc = DgramSocket_readAvailable(s2, false);
			if (rc != 1000) { err(); return 1; }
			rc = DgramSocket_read(s2, buf, 1000);
			if (rc != 0) { err(); return 1; }
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			DgramSocket_destroy(s3);
			return 0;
		} break;
		case 21: {
			uint8_t *mac1 = (uint8_t *)buf+10000;
			uint8_t *mac2 = (uint8_t *)buf+10006;
			uint8_t *fr1 = (uint8_t *)buf+0;
			uint8_t *fr2 = (uint8_t *)buf+2000;
			uint8_t *fr1_mac_s = fr1+6;
			uint8_t *fr1_mac_d = fr1+0;
			uint8_t *fr1_type = fr1+12;
			uint8_t *fr1_data = fr1+14;
			uint8_t *fr2_mac_s = fr2+6;
			uint8_t *fr2_mac_d = fr2+0;
			uint8_t *fr2_type = fr2+12;
			uint8_t *fr2_data = fr2+14;
			prepEthFrame(fr1);
			s1 = EtherDgramSocket_create(iface, 0);
			s2 = EtherDgramSocket_create(iface, 0);
			// clean
			DgramSocket_destroy(s1);
			DgramSocket_destroy(s2);
			return 0;
		} break;
		// EtherDgramSocket_create
		// EtherDgramSocket_setHeader
		// EtherDgramSocket_getHeader
		// EtherDgramSocket_getInterfaceMACAddress
		// EtherDgramSocket_getSocketMACAddress
	}

	{ err(); return 1; }
}