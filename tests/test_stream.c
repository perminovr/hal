
#include <stdio.h>
#include "hal_socket_stream.h"
#include "hal_poll.h"
#include "hal_time.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
	int rc;
	char buf[65535];
	ServerSocket s;
	ClientSocket c1, c2;
	ClientSocket cs1, cs2;
	union uClientSocketAddress addr;

    switch (test) {
        case 1: { // tcp base
			// link
			s = TcpServerSocket_create(1, "127.0.0.1", 43555);
			ServerSocket_listen(s, 1);
			c1 = TcpClientSocket_create();
			strcpy(addr.ip, "127.0.0.1");
			addr.port = 43555;
			rc = (int)ClientSocket_connectAsync(c1, &addr);
			if (rc != 1) { err(); return 1; }
			cs1 = ServerSocket_accept(s);
			if (cs1 == NULL) { err(); return 1; }
			// just call it
			TcpClientSocket_activateTcpKeepAlive(c1, 1,1,1);
			TcpClientSocket_activateTcpNoDelay(c1);
			TcpClientSocket_setUnacknowledgedTimeout(c1, 1);
			// write
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = ClientSocket_write(c1, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = ClientSocket_write(c1, buf+1000, 100);
			if (rc != 100) { err(); return 1; }
			rc = ClientSocket_write(c1, buf+1100, 10);
			if (rc != 10) { err(); return 1; }
			rc = ClientSocket_write(c1, buf+1110, 1);
			if (rc != 1) { err(); return 1; }
			// read available
			rc = ClientSocket_readAvailable(cs1);
			if (rc != 1111) { err(); return 1; }
			// check poll
			int revents;
			uint64_t ts, ts0;
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(ClientSocket_getDescriptor(cs1), HAL_POLLIN|HAL_POLLOUT, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) == 0) { err(); return 1; }
			if ( (revents&HAL_POLLOUT) == 0) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// read
			memset(buf, 0, 65535);
			rc = ClientSocket_read(cs1, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = ClientSocket_read(cs1, buf+1000, 100);
			if (rc != 100) { err(); return 1; }
			rc = ClientSocket_read(cs1, buf+1100, 10);
			if (rc != 10) { err(); return 1; }
			rc = ClientSocket_read(cs1, buf+1110, 1);
			if (rc != 1) { err(); return 1; }
            for (int i = 0; i < 1111; ++i) {
                if (buf[i] != (char)i) { err(); return 1; }
            }
			// clean
			ClientSocket_destroy(c1);
			ClientSocket_destroy(cs1);
			ServerSocket_destroy(s);
			return 0;
        } break;
        case 2: { // accept two con
			// link
			s = TcpServerSocket_create(2, "127.0.0.1", 43555);
			ServerSocket_listen(s, 1);
			c1 = TcpClientSocket_create();
			strcpy(addr.ip, "127.0.0.1");
			addr.port = 43555;
			rc = (int)ClientSocket_connectAsync(c1, &addr);
			if (rc != 1) { err(); return 1; }
			cs1 = ServerSocket_accept(s);
			if (cs1 == NULL) { err(); return 1; }
			c2 = TcpClientSocket_create();
			rc = (int)ClientSocket_connectAsync(c2, &addr);
			if (rc != 1) { err(); return 1; }
			cs2 = ServerSocket_accept(s);
			if (cs2 == NULL) { err(); return 1; }
			// check
			if (ServerSocket_getClientsNumber(s) != 2) { err(); return 1; }
			ServerClient sc = ServerSocket_getClients(s);
			if (sc->self != cs1) { err(); return 1; }
			if (sc->next->self != cs2) { err(); return 1; }
			if (sc->next->next != NULL) { err(); return 1; }
			if (ClientSocket_checkConnectState(c1) != SOCKET_STATE_CONNECTED) { err(); return 1; }
			if (ClientSocket_checkConnectState(c2) != SOCKET_STATE_CONNECTED) { err(); return 1; }
			if (ClientSocket_reset(c1) != true) { err(); return 1; }
			if (ClientSocket_checkConnectState(c1) == SOCKET_STATE_CONNECTED) { err(); return 1; }
			// clean
			ClientSocket_destroy(c2);
			ClientSocket_destroy(cs2);
			ClientSocket_destroy(cs1);
			ServerSocket_closeClients(s); // just test
			ClientSocket_destroy(c1);
			ServerSocket_destroy(s);
			return 0;
        } break;
        case 3: { // clients bind
			// link
			s = TcpServerSocket_create(2, "127.0.0.1", 43555);
			ServerSocket_listen(s, 1);
			c1 = TcpClientSocket_createAndBind("127.0.0.1", 43556);
			strcpy(addr.ip, "127.0.0.1");
			addr.port = 43555;
			rc = (int)ClientSocket_connectAsync(c1, &addr);
			if (rc != 1) { err(); return 1; }
			cs1 = ServerSocket_accept(s);
			if (cs1 == NULL) { err(); return 1; }
			c2 = TcpClientSocket_createAndBind("127.0.0.1", 43557);
			rc = (int)ClientSocket_connectAsync(c2, &addr);
			if (rc != 1) { err(); return 1; }
			cs2 = ServerSocket_accept(s);
			if (cs2 == NULL) { err(); return 1; }
			// check addr
			memset(&addr, 0, sizeof(addr));
			// on client side
			rc = (int)ClientSocket_getPeerAddress(c1, &addr);
			if (rc != 1) { err(); return 1; }
			if (addr.port != 43555) { err(); return 1; }
			rc = (int)ClientSocket_getLocalAddress(c1, &addr);
			if (rc != 1) { err(); return 1; }
			if (addr.port != 43556) { err(); return 1; }
			// on server side
			rc = (int)ClientSocket_getPeerAddress(cs1, &addr);
			if (rc != 1) { err(); return 1; }
			if (addr.port != 43556) { err(); return 1; }
			rc = (int)ClientSocket_getLocalAddress(cs1, &addr);
			if (rc != 1) { err(); return 1; }
			if (addr.port != 43555) { err(); return 1; }
			// clean
			ClientSocket_destroy(c2);
			ServerSocket_closeClients(s);
			ClientSocket_destroy(c1);
			ServerSocket_destroy(s);
			return 0;
        } break;
		case 4: { // sync con
			// prep
			c1 = TcpClientSocket_create();
			strcpy(addr.ip, "127.0.0.1");
			addr.port = 43555;
			// timeout
			rc = (int)ClientSocket_connect(c1, &addr, 100);
			if (rc != 0) { err(); return 1; }
			if (ClientSocket_checkConnectState(c1) == SOCKET_STATE_CONNECTED) { err(); return 1; }
			if (ClientSocket_reset(c1) == false) { err(); return 1; }
			// run server
			s = TcpServerSocket_create(1, "127.0.0.1", 43555);
			ServerSocket_listen(s, 1);
			// server waits for us
			rc = (int)ClientSocket_connect(c1, &addr, 100);
			if (rc == 0) { err(); return 1; }
			if (ClientSocket_checkConnectState(c1) != SOCKET_STATE_CONNECTED) { err(); return 1; }
			// clean
			ClientSocket_destroy(c1);
			ServerSocket_closeClients(s);
			ServerSocket_destroy(s);
			return 0;
		} break;
		case 5: {
			// link
			s = TcpServerSocket_create(1, "127.0.0.1", 43555);
			ServerSocket_listen(s, 1);
			c1 = TcpClientSocket_create();
			strcpy(addr.ip, "127.0.0.1");
			addr.port = 43555;
			rc = (int)ClientSocket_connectAsync(c1, &addr);
			if (rc != 1) { err(); return 1; }
			cs1 = ServerSocket_accept(s);
			if (cs1 == NULL) { err(); return 1; }
			c2 = TcpClientSocket_create();
			rc = (int)ClientSocket_connectAsync(c2, &addr);
			if (rc != 1) { err(); return 1; }
			cs2 = ServerSocket_accept(s);
			if (cs2 != NULL) { err(); return 1; }
			// clean
			ClientSocket_destroy(c2);
			ClientSocket_destroy(cs1);
			ClientSocket_destroy(c1);
			ServerSocket_destroy(s);
			return 0;
        } break;
        case 100: { // local base
			// link
			s = LocalServerSocket_create(1, "/tmp/local-s-test");
			ServerSocket_listen(s, 1);
			c1 = LocalClientSocket_create();
			strcpy(addr.address, "/tmp/local-s-test");
			rc = (int)ClientSocket_connectAsync(c1, &addr);
			if (rc != 1) { err(); return 1; }
			cs1 = ServerSocket_accept(s);
			if (cs1 == NULL) { err(); return 1; }
			// write
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = ClientSocket_write(c1, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = ClientSocket_write(c1, buf+1000, 100);
			if (rc != 100) { err(); return 1; }
			rc = ClientSocket_write(c1, buf+1100, 10);
			if (rc != 10) { err(); return 1; }
			rc = ClientSocket_write(c1, buf+1110, 1);
			if (rc != 1) { err(); return 1; }
			// read
			rc = ClientSocket_readAvailable(cs1);
			if (rc != 1111) { err(); return 1; }
			memset(buf, 0, 65535);
			rc = ClientSocket_read(cs1, buf, 1000);
			if (rc != 1000) { err(); return 1; }
			rc = ClientSocket_read(cs1, buf+1000, 100);
			if (rc != 100) { err(); return 1; }
			rc = ClientSocket_read(cs1, buf+1100, 10);
			if (rc != 10) { err(); return 1; }
			rc = ClientSocket_read(cs1, buf+1110, 1);
			if (rc != 1) { err(); return 1; }
            for (int i = 0; i < 1111; ++i) {
                if (buf[i] != (char)i) { err(); return 1; }
            }
			// clean
			ClientSocket_destroy(c1);
			ClientSocket_destroy(cs1);
			ServerSocket_destroy(s);
			return 0;
        } break;
        case 101: { // accept two con
			// link
			s = LocalServerSocket_create(2, "/tmp/local-s-test");
			ServerSocket_listen(s, 1);
			c1 = LocalClientSocket_create();
			strcpy(addr.address, "/tmp/local-s-test");
			rc = (int)ClientSocket_connectAsync(c1, &addr);
			if (rc != 1) { err(); return 1; }
			cs1 = ServerSocket_accept(s);
			if (cs1 == NULL) { err(); return 1; }
			c2 = LocalClientSocket_create();
			rc = (int)ClientSocket_connectAsync(c2, &addr);
			if (rc != 1) { err(); return 1; }
			cs2 = ServerSocket_accept(s);
			if (cs2 == NULL) { err(); return 1; }
			// check
			if (ServerSocket_getClientsNumber(s) != 2) { err(); return 1; }
			ServerClient sc = ServerSocket_getClients(s);
			if (sc->self != cs1) { err(); return 1; }
			if (sc->next->self != cs2) { err(); return 1; }
			if (sc->next->next != NULL) { err(); return 1; }
			if (ClientSocket_checkConnectState(c1) != SOCKET_STATE_CONNECTED) { err(); return 1; }
			if (ClientSocket_checkConnectState(c2) != SOCKET_STATE_CONNECTED) { err(); return 1; }
			if (ClientSocket_reset(c1) != true) { err(); return 1; }
			if (ClientSocket_checkConnectState(c1) == SOCKET_STATE_CONNECTED) { err(); return 1; }
			// clean
			ClientSocket_destroy(c2);
			ClientSocket_destroy(cs2);
			ClientSocket_destroy(cs1);
			ServerSocket_closeClients(s); // just test
			ClientSocket_destroy(c1);
			ServerSocket_destroy(s);
			return 0;
        } break;
        case 102: { // close second con
			// link
			s = LocalServerSocket_create(1, "/tmp/local-s-test");
			ServerSocket_listen(s, 1);
			c1 = LocalClientSocket_create();
			strcpy(addr.address, "/tmp/local-s-test");
			rc = (int)ClientSocket_connectAsync(c1, &addr);
			if (rc != 1) { err(); return 1; }
			// check server poll
			int revents;
			uint64_t ts, ts0;
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(ServerSocket_getDescriptor(s), HAL_POLLIN, &revents, 100);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			// accept
			cs1 = ServerSocket_accept(s);
			if (cs1 == NULL) { err(); return 1; }
			c2 = LocalClientSocket_create();
			rc = (int)ClientSocket_connectAsync(c2, &addr);
			if (rc != 1) { err(); return 1; }
			cs2 = ServerSocket_accept(s);
			if (cs2 != NULL) { err(); return 1; }
			// clean
			ClientSocket_destroy(c2);
			ClientSocket_destroy(cs1);
			ClientSocket_destroy(c1);
			ServerSocket_destroy(s);
			return 0;
        } break;
    }

    { err(); return 1; }
}