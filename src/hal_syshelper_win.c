
#if defined(_WIN32) || defined(_WIN64)

#include <winsock2.h>
#include <windows.h>

#include "hal_syshelper.h"
#include "hal_poll.h"
#include "hal_thread.h"



struct sShDescStreamFd {
	unidesc h; // system object
	struct { // helper socks
		ServerSocket s;
		ClientSocket i; // internal
		ClientSocket e; // external
	};
};
typedef struct sShDescStreamFd * ShDescStreamFd;

struct sShDescStream {
	ShDescStreamFd fd;
	Thread thr;
	uint8_t *buf;
	uint32_t bufSize;
	int pauseCharMs;

	void *user;
	hsfh_spec_rw_cb_t rcb;
	hsfh_spec_rw_cb_t wcb;
};


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

static ShDescStreamFd ShDescStreamFd_create(unidesc h)
{
	ServerSocket s;
	ClientSocket e, i;
	char server_name[MAX_PATH];
	union uClientSocketAddress addr;
	ShDescStreamFd ret;

	if ( (s = getLocalServer(server_name)) == NULL) goto exit_error;

	e = LocalClientSocket_create();
	if (e == NULL) goto exit_error;
	strncpy(addr.address, server_name, sizeof(addr.address)-1);
	ClientSocket_connectAsync(e, &addr);
	if ( Hal_pollSingle(ServerSocket_getDescriptor(s), HAL_POLLIN, NULL, 100) <= 0 )
		goto exit_error;
	i = ServerSocket_accept(s);
	if (i == NULL) goto exit_error;

	ret = (ShDescStreamFd)calloc(1, sizeof(struct sShDescStreamFd));
	if (ret) {
		ret->h = h;
		ret->e = e;
		ret->s = s;
		ret->i = i;
		return ret;
	}

exit_error:
	ClientSocket_destroy(e);
	ServerSocket_destroy(s);
	return NULL;
}

static void ShDescStreamFd_destroy(ShDescStreamFd self)
{
	if (!self) return;
	ClientSocket_destroy(self->e);
	ServerSocket_destroy(self->s);
	free(self);
}


static inline void abortStreamLoop(ShDescStream self)
{
	ClientSocket_destroy(self->fd->i); // emit application about broken handle
	self->fd->i = NULL;
	Thread_testCancel(self->thr); // clear before
	ExitThread(0); // nothing canbe done
}

static void *ShDescStream_loop(void *p)
{
	ShDescStream self = (ShDescStream)p;
	uint64_t ifd = ClientSocket_getDescriptor(self->fd->i).u64;
	HANDLE hset[3] = {
		(HANDLE)(Signal_getDescriptor(Thread_getCancelSignal(self->thr)).u64),
		(HANDLE)(ifd),
		(HANDLE)(self->fd->h.u64),
	};
	DWORD rc, i, it, bytes;
	hsfh_spec_rw_cb_t rcb;
	hsfh_spec_rw_cb_t wcb;
	for (;;) {
		rc = WaitForMultipleObjects(3, hset, FALSE, INFINITE);
		if (rc != WAIT_TIMEOUT && rc != WAIT_FAILED) {
			if (rc < WAIT_ABANDONED_0) {
				i = rc - WAIT_OBJECT_0;
				if (i < 3) {
					HANDLE rh = hset[i], wh = INVALID_HANDLE_VALUE;
					switch (i) {
						case 1: { // read internal sock; send data using sys handle
							wh = (HANDLE)(self->fd->h.u64);
							rcb = NULL;
							wcb = self->wcb;
						} break;
						case 2: { // read sys handle; send data using internal sock (to external)
							wh = (HANDLE)(ifd);
							rcb = self->rcb;
							wcb = NULL;
						} break;
						default: break;
					}
					if (wh != INVALID_HANDLE_VALUE) {
						it = 0;
						rc = 0;
						while (1) {
							// todo broken handle
							if (WaitForSingleObject(rh, 0) == WAIT_OBJECT_0) { // check the data is available
								if (rcb) { // special reader
									int r = rcb(self->user, (uint8_t *)(self->buf), (int)self->bufSize);
									if (r > 0) {
										it = (DWORD)r;
										break;
									}
									abortStreamLoop(self);
								} else { // system reader
									bytes = 0;
									if (ReadFile(rh, (LPVOID)(self->buf+it), 1, &bytes, NULL) == FALSE) {
										abortStreamLoop(self);
									}
									if (bytes != 1) break;
									if (++it >= self->bufSize) break;
									rc = 0; // byte received we can wait a little for another one (pauseCharMs)
								}
							} else {
								if (rc == 0 && i == 2 && self->pauseCharMs > 0) { // only for sys handle
									Thread_sleep(self->pauseCharMs);
									rc = 1;
								}
								else break;
							}
						}
						if (it > 0) {
							if (wcb) {
								if (wcb(self->user, (uint8_t *)(self->buf), (int)it) <= 0) {
									abortStreamLoop(self);
								}
							} else {
								bytes = 0;
								WriteFile(wh, (LPVOID)self->buf, it, &bytes, NULL); // todo
								FlushFileBuffers(wh);
							}
						}
					}
				}
			}
		}
		Thread_testCancel(self->thr);
	}
}

ShDescStream ShDescStream_create(unidesc h, uint32_t rwBufSize, int pauseCharMs)
{
	ShDescStream self = calloc(1, sizeof(struct sShDescStream));
	if (!self) return NULL;

	self->buf = (uint8_t *)calloc(1, rwBufSize);
	if (!self->buf) goto exit_err;
	self->bufSize = (uint32_t)rwBufSize;

	self->fd = ShDescStreamFd_create(h);
	if (!self->fd) goto exit_err;

	self->thr = Thread_create(0xffff, ShDescStream_loop, self, false);
	if (!self->thr) goto exit_err;

	self->pauseCharMs = pauseCharMs;

	Thread_start(self->thr);

	return self;

exit_err:
	ShDescStream_destroy(self);
	return NULL;
}

int ShDescStream_write(ShDescStream self, const uint8_t *buf, int size)
{
	if (!self) return -1;
	return ClientSocket_write(self->fd->e, buf, size);
}

int ShDescStream_read(ShDescStream self, uint8_t *buf, int size)
{
	if (!self) return -1;
	return ClientSocket_read(self->fd->e, buf, size);
}

void ShDescStream_setSpecCallbacks(ShDescStream self, void *user, hsfh_spec_rw_cb_t rcb, hsfh_spec_rw_cb_t wcb)
{
	if (!self) return;
	self->user = user;
	self->rcb = rcb;
	self->wcb = wcb;
}

void ShDescStream_destroy(ShDescStream self)
{
	if (!self) return;
	Thread_cancel(self->thr);
	Thread_destroy(self->thr);
	ShDescStreamFd_destroy(self->fd);
	if (self->buf) free(self->buf);
	free(self);
}

unidesc ShDescStream_getDescriptor(ShDescStream self)
{
	if (self) {
		return ClientSocket_getDescriptor(self->fd->e);
	}
	return Hal_getInvalidUnidesc();
}



struct sShDescDgramFd {
	unidesc h; // system object
	struct { // helper socks
		DgramSocket i; // internal
		DgramSocket e; // external
	};
};
typedef struct sShDescDgramFd * ShDescDgramFd;

struct sShDescDgram {
	ShDescDgramFd fd;
	Thread thr;
	uint8_t *buf;
	uint32_t bufSize;

	void *user;
	hsfh_spec_rw_cb_t rcb;
	hsfh_spec_rw_cb_t wcb;
};


static DgramSocket getLocalSocket(char *buf)
{
	DgramSocket s = NULL;
	int attempts = 100;
	while (attempts--) {
		if (GetTempFileNameA((LPCSTR)"\\.", (LPCSTR)"soc", 0, (LPSTR)buf) == 0) return NULL;
		LocalDgramSocket_unlinkAddress(buf);
		s = LocalDgramSocket_create(buf);
		if (s) { return s; }
		Thread_sleep(1);
	}
	return NULL;
}

ShDescDgramFd ShDescDgramFd_create(unidesc h)
{
	DgramSocket e, i;
	char e_name[MAX_PATH];
	char i_name[MAX_PATH];
	union uDgramSocketAddress addr;
	ShDescDgramFd ret;

	if ( (e = getLocalSocket(e_name)) == NULL) goto exit_error;
	if ( (i = getLocalSocket(i_name)) == NULL) goto exit_error;

	strncpy(addr.address, e_name, sizeof(addr.address)-1);
	DgramSocket_setRemote(i, &addr);
	strncpy(addr.address, i_name, sizeof(addr.address)-1);
	DgramSocket_setRemote(e, &addr);

	ret = (ShDescDgramFd)calloc(1, sizeof(struct sShDescDgramFd));
	if (ret) {
		ret->h = h;
		ret->e = e;
		ret->i = i;
		return ret;
	}

exit_error:
	DgramSocket_destroy(e);
	DgramSocket_destroy(i);
	return NULL;
}

void ShDescDgramFd_destroy(ShDescDgramFd self)
{
	if (!self) return;
	DgramSocket_destroy(self->e);
	DgramSocket_destroy(self->i);
	free(self);
}


HAL_INTERNAL void DgramSocket_close(DgramSocket self);
static inline void abortDgramLoop(ShDescDgram self)
{
	DgramSocket_close(self->fd->e); // emit application about broken handle
	Thread_testCancel(self->thr); // clear before
	ExitThread(0); // nothing canbe done
}

static void *ShDescDgram_loop(void *p)
{
	ShDescDgram self = (ShDescDgram)p;
	uint64_t ifd = DgramSocket_getDescriptor(self->fd->i).u64;
	HANDLE hset[3] = {
		(HANDLE)(Signal_getDescriptor(Thread_getCancelSignal(self->thr)).u64),
		(HANDLE)(ifd),
		(HANDLE)(self->fd->h.u64),
	};
	DWORD rc, i, it, bytes;
	hsfh_spec_rw_cb_t rcb;
	hsfh_spec_rw_cb_t wcb;
	for (;;) {
		rc = WaitForMultipleObjects(3, hset, FALSE, INFINITE);
		if (rc != WAIT_TIMEOUT && rc != WAIT_FAILED) {
			if (rc < WAIT_ABANDONED_0) {
				i = rc - WAIT_OBJECT_0;
				if (i < 3) {
					HANDLE rh = hset[i], wh = INVALID_HANDLE_VALUE;
					switch (i) {
						case 1: { // read internal sock; send data using sys handle
							wh = (HANDLE)(self->fd->h.u64);
							rcb = NULL;
							wcb = self->wcb;
						} break;
						case 2: { // read sys handle; send data using internal sock (to external)
							wh = (HANDLE)(ifd);
							rcb = self->rcb;
							wcb = NULL;
						} break;
						default: break;
					}
					if (wh != INVALID_HANDLE_VALUE) {
						it = 0;
						rc = 0;
						while (1) {
							// todo broken handle
							if (WaitForSingleObject(rh, 0) == WAIT_OBJECT_0) { // check the data is available
								if (rcb) { // special reader
									int r = rcb(self->user, (uint8_t *)(self->buf), (int)self->bufSize);
									if (r > 0) {
										it = (DWORD)r;
										break;
									}
									abortDgramLoop(self);
								} else { // system reader
									bytes = 0;
									if (ReadFile(rh, (LPVOID)(self->buf+it), self->bufSize, &bytes, NULL) == FALSE) {
										abortDgramLoop(self);
									}
									it = bytes;
									break;
								}
							} else break;
						}
						if (it > 0) {
							if (wcb) {
								if (wcb(self->user, (uint8_t *)(self->buf), (int)it) <= 0) {
									abortDgramLoop(self);
								}
							} else {
								bytes = 0;
								WriteFile(wh, (LPVOID)self->buf, it, &bytes, NULL); // todo
								FlushFileBuffers(wh);
							}
						}
					}
				}
			}
		}
		Thread_testCancel(self->thr);
	}
}

ShDescDgram ShDescDgram_create(unidesc h, uint32_t rwBufSize)
{
	ShDescDgram self = calloc(1, sizeof(struct sShDescDgram));
	if (!self) return NULL;

	self->buf = (uint8_t *)calloc(1, rwBufSize);
	if (!self->buf) goto exit_err;
	self->bufSize = (uint32_t)rwBufSize;

	self->fd = ShDescDgramFd_create(h);
	if (!self->fd) goto exit_err;

	self->thr = Thread_create(0xffff, ShDescDgram_loop, self, false);
	if (!self->thr) goto exit_err;

	Thread_start(self->thr);

	return self;

exit_err:
	ShDescDgram_destroy(self);
	return NULL;
}

int ShDescDgram_write(ShDescDgram self, const uint8_t *buf, int size)
{
	if (!self) return -1;
	return DgramSocket_write(self->fd->e, buf, size);
}

int ShDescDgram_read(ShDescDgram self, uint8_t *buf, int size)
{
	if (!self) return -1;
	return DgramSocket_read(self->fd->e, buf, size);
}

void ShDescDgram_setSpecCallbacks(ShDescDgram self, void *user, hsfh_spec_rw_cb_t rcb, hsfh_spec_rw_cb_t wcb)
{
	if (!self) return;
	self->user = user;
	self->rcb = rcb;
	self->wcb = wcb;
}

void ShDescDgram_destroy(ShDescDgram self)
{
	if (!self) return;
	Thread_cancel(self->thr);
	Thread_destroy(self->thr);
	ShDescDgramFd_destroy(self->fd);
	if (self->buf) free(self->buf);
	free(self);
}

unidesc ShDescDgram_getDescriptor(ShDescDgram self)
{
	if (self) {
		return DgramSocket_getDescriptor(self->fd->e);
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