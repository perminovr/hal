
#if defined(_WIN32) || defined(_WIN64)

#include <winsock2.h>
#include <windows.h>

#include "hal_syshelper.h"
#include "hal_poll.h"
#include "hal_thread.h"


struct sHalShFdescHelper {
	HalShFdesc fd;
	Thread thr;
	Signal wkupSig;
	uint8_t *buf;
	uint32_t bufSize;
	int pauseCharMs;
};

static void *HalShFdescHelper_loop(void *p)
{
	HalShFdescHelper self = (HalShFdescHelper)p;
	uint64_t ifd = ClientSocket_getDescriptor(self->fd->i).u64;
	HANDLE hset[3] = {
		(HANDLE)(Signal_getDescriptor(self->wkupSig).u64),
		(HANDLE)(ifd),
		(HANDLE)(self->fd->h.u64),
	};
	DWORD rc, i, it, bytes;
	for (;;) {
		rc = WaitForMultipleObjects(3, hset, FALSE, INFINITE);
		if (rc != WAIT_TIMEOUT && rc != WAIT_FAILED) {
			if (rc < WAIT_ABANDONED_0) {
				i = rc - WAIT_OBJECT_0;
				if (i < 3) {
					HANDLE rh = hset[i], wh = INVALID_HANDLE_VALUE;
					switch (i) {
						case 0: {
							Signal_end(self->wkupSig);
						} break;
						case 1: { // read internal sock; send data using sys handle
							wh = (HANDLE)(self->fd->h.u64);
						} break;
						case 2: { // read sys handle; send data using internal sock (to external)
							wh = (HANDLE)(ifd);
						} break;
						default: break;
					}
					if (wh != INVALID_HANDLE_VALUE) {
						it = 0;
						rc = 0;
						while (1) {
							if (WaitForSingleObject(rh, 0) == WAIT_OBJECT_0) { // check the data is available
								bytes = 0;
								if (ReadFile(rh, (LPVOID)(self->buf+it), 1, &bytes, NULL) == FALSE) {
									ClientSocket_destroy(self->fd->i); // emit application about broken handle
									self->fd->i = NULL;
									Thread_testCancel(self->thr); // clear before
									ExitThread(0); // nothing canbe done
								}
								if (bytes != 1) break;
								if (++it >= self->bufSize) break;
								rc = 0; // byte received we can wait a little for another one (pauseCharMs)
							} else {
								if (rc == 0 && i == 2 && self->pauseCharMs > 0) { // only for sys handle
									Thread_sleep(self->pauseCharMs);
									rc = 1;
								}
								else break;
							}
						}
						if (it > 0) {
							bytes = 0;
							WriteFile(wh, (LPVOID)self->buf, it, &bytes, NULL);
							FlushFileBuffers(wh);
						}
					}
				}
			}
		}
		Thread_testCancel(self->thr);
	}
}

HalShFdescHelper HalShFdescHelper_create(unidesc h, uint32_t wrBufSize, int pauseCharMs)
{
	HalShFdescHelper self = calloc(1, sizeof(struct sHalShFdescHelper));
	if (!self) return NULL;

	self->buf = (uint8_t *)calloc(1, wrBufSize);
	if (!self->buf) goto exit_err;
	self->bufSize = (uint32_t)wrBufSize;

	self->fd = HalShFdesc_create(h);
	if (!self->fd) goto exit_err;

	self->wkupSig = Signal_create();
	if (!self->wkupSig) goto exit_err;

	self->thr = Thread_create(0xffff, HalShFdescHelper_loop, self, false);
	if (!self->fd) goto exit_err;

	self->pauseCharMs = pauseCharMs;

	Thread_start(self->thr);

	return self;

exit_err:
	HalShFdescHelper_destroy(self);
	return NULL;
}

int HalShFdescHelper_write(HalShFdescHelper self, const uint8_t *buf, int size)
{
	if (!self) return -1;
	return ClientSocket_write(self->fd->e, buf, size);
}

int HalShFdescHelper_read(HalShFdescHelper self, uint8_t *buf, int size)
{
	if (!self) return -1;
	return ClientSocket_read(self->fd->e, buf, size);
}

void HalShFdescHelper_destroy(HalShFdescHelper self)
{
	if (!self) return;
	if (self->thr) {
		Thread_cancel(self->thr);
		Signal_raise(self->wkupSig);
		Thread_destroy(self->thr);
	}
	if (self->fd) HalShFdesc_destroy(self->fd);
	if (self->wkupSig) Signal_destroy(self->wkupSig);
	if (self->buf) free(self->buf);
	free(self);
}

HalShFdesc HalShFdescHelper_getDescriptor(HalShFdescHelper self)
{
	if (!self) return NULL;
	return self->fd;
}


static ServerSocket getLocalServer(char *buf)
{
	ServerSocket s = NULL;
	int attempts = 100;
	while (attempts--) {
		if (GetTempFileNameA((LPCSTR)"\\.", (LPCSTR)"soc", 0, (LPSTR)buf) == 0) return NULL;
		LocalServerSocket_unlinkAddress(buf);
		s = LocalServerSocket_create(1, buf);
		if (s) {
			ServerSocket_listen(s, 1);
			return s;
		}
		Thread_sleep(1);
	}
	return NULL;
}

HalShFdesc HalShFdesc_create(unidesc h)
{
	ServerSocket s;
	ClientSocket e, i;
	char server_name[MAX_PATH];
	union uClientSocketAddress addr;

	if ( (s = getLocalServer(server_name)) == NULL) goto exit_error;

	e = LocalClientSocket_create();
	if (e == NULL) goto exit_error;
	strncpy(addr.address, server_name, sizeof(addr.address)-1);
	ClientSocket_connectAsync(e, &addr);
	if ( Hal_pollSingle(ServerSocket_getDescriptor(s), HAL_POLLIN, NULL, 100) <= 0 )
		goto exit_error;
	i = ServerSocket_accept(s);
	if (i == NULL) goto exit_error;

	HalShFdesc ret = (HalShFdesc)calloc(1, sizeof(struct sHalShFdesc));
	if (ret) {
		ret->h = h;
		ret->e = e;
		ret->s = s;
		ret->i = i;
		return ret;
	}

exit_error:
	if (e) ClientSocket_destroy(e);
	if (s) ServerSocket_destroy(s);
	return NULL;
}

void HalShFdesc_destroy(HalShFdesc self)
{
	if (!self) return;
	if (self->e) ClientSocket_destroy(self->e);
	if (self->s) ServerSocket_destroy(self->s);
	free(self);
}

unidesc HalShFdesc_getDescriptor(HalShFdesc self)
{
	if (self) {
		return ClientSocket_getDescriptor(self->e);
	}
	return Hal_getInvalidUnidesc();
}


static uint32_t HalShSys_initCnt = 0;

void HalShSys_init(void)
{
	if (HalShSys_initCnt == 0) {
		WSADATA wsa;
		WORD ver = MAKEWORD(2,2);
		timeBeginPeriod(1);
		WSAStartup(ver, &wsa);
	}
	HalShSys_initCnt++;
}

void HalShSys_deinit(void)
{
	if (HalShSys_initCnt) {
		HalShSys_initCnt--;
		if (HalShSys_initCnt == 0) {
			WSACleanup();
			timeEndPeriod(1);
		}
	}
}


#endif // _WIN32 || _WIN64