
#if defined(_WIN32) || defined(_WIN64)

#include "hal_thread.h"
#include "hal_socket_dgram.h"
#include <winsock2.h>
#include <windows.h>


struct sThread {
	ThreadExecutionFunction function;
	void *parameter;
	HANDLE handle;
	DWORD threadId;
	int state;
	bool autodestroy;
	Signal cs;
	Signal ps;
	Semaphore sem1;
	Semaphore sem2;
};


Semaphore Semaphore_create(int initialValue)
{
	HANDLE h = CreateSemaphore(NULL, (LONG)initialValue, 1, NULL);
	return ((h == INVALID_HANDLE_VALUE)? NULL : (Semaphore)h);
}

void Semaphore_wait(Semaphore self)
{
	if (self == NULL) return;
	WaitForSingleObject((HANDLE)self, INFINITE);
}

void Semaphore_post(Semaphore self)
{
	if (self == NULL) return;
	ReleaseSemaphore((HANDLE)self, 1, NULL);
}

void Semaphore_destroy(Semaphore self)
{
	if (self == NULL) return;
	CloseHandle((HANDLE)self);
}


Mutex Mutex_create(void)
{
	HANDLE h = CreateMutex(NULL, FALSE, NULL);
	return ((h == INVALID_HANDLE_VALUE)? NULL : (Mutex)h);
}

void Mutex_lock(Mutex self)
{
	if (self == NULL) return;
	WaitForSingleObject((HANDLE)self, INFINITE);
}

int Mutex_trylock(Mutex self)
{
	return Mutex_timedlock(self, 0);
}

int Mutex_timedlock(Mutex self, int millies)
{
	if (self == NULL) return -1;
	DWORD rc = WaitForSingleObject((HANDLE)self, (DWORD)millies);
	return (rc == WAIT_OBJECT_0)? 0 : -1;
}

void Mutex_unlock(Mutex self)
{
	if (self == NULL) return;
	ReleaseMutex((HANDLE)self);
}

void Mutex_destroy(Mutex self)
{
	if (self == NULL) return;
	CloseHandle((HANDLE)self);
}


static void Thread_destroyMem(Thread self)
{
	Semaphore_destroy(self->sem2);
	Semaphore_destroy(self->sem1);
	Signal_destroy(self->ps);
	Signal_destroy(self->cs);
	free(self);
}

static DWORD WINAPI destroyAutomaticThreadRunner(LPVOID parameter)
{
	Thread self = (Thread) parameter;
	self->function(self->parameter);
	self->state = 0;
	CloseHandle(self->handle);
	Thread_destroyMem(self);
	return 0;
}

static DWORD WINAPI threadRunner(LPVOID parameter)
{
	Thread self = (Thread) parameter;
	return (DWORD)((size_t)self->function(self->parameter));
}

Thread Thread_create(size_t stackSize, ThreadExecutionFunction function, void *parameter, bool autodestroy)
{
	Thread self = (Thread) calloc(1, sizeof(struct sThread));
	if (!self) return NULL;

	self->cs = Signal_create();
	if (!self->cs) {
		goto exit_self;
	}

	self->ps = Signal_create();
	if (!self->ps) {
		goto exit_cs;
	}

	self->sem1 = Semaphore_create(1);
	if (!self->sem1) {
		goto exit_ps;
	}

	self->sem2 = Semaphore_create(1);
	if (!self->sem2) {
		goto exit_sem1;
	}

	self->parameter = parameter;
	self->function = function;
	self->state = 0;
	self->autodestroy = autodestroy;
	self->handle = CreateThread(
			0, (SIZE_T)stackSize,
			((autodestroy)? destroyAutomaticThreadRunner : threadRunner),
			(LPVOID)self, CREATE_SUSPENDED, (LPDWORD)&self->threadId
	);

	return self;

exit_sem1:
	Semaphore_destroy(self->sem1);
exit_ps:
	Signal_destroy(self->ps);
exit_cs:
	Signal_destroy(self->cs);
exit_self:
	free(self);
	return NULL;
}

void Thread_start(Thread self)
{
	if (self == NULL) return;
	self->state = 1;
	ResumeThread(self->handle);
}

void Thread_destroy(Thread self)
{
	if (self == NULL) return;
	if (self->autodestroy) {
		Signal_raise(self->cs);
		return; // can't do anything else
	}
	if (self->state == 1) {
		WaitForSingleObject(self->handle, INFINITE);
	}
	CloseHandle(self->handle);
	Thread_destroyMem(self);
}

void Thread_sleep(int millies)
{
	Sleep(millies);
}

void Thread_yield(void)
{
	SwitchToThread();
	YieldProcessor();
}

void Thread_cancel(Thread self)
{
	if (self == NULL) return;
	Signal_raise(self->cs);
}

void Thread_testCancel(Thread self)
{
	if (self == NULL) return;
	if (Signal_event(self->cs)) {
		Signal_end(self->cs);
		if (self->autodestroy)
			return;
		ExitThread(0);
	}
}

void Thread_testPause(Thread self)
{
	if (Signal_event(self->ps)) {
		Signal_end(self->ps);
		Semaphore_post(self->sem2); // unlock pause
		Semaphore_wait(self->sem1); // waiting for resume
		Semaphore_post(self->sem1); // free resource
	}
}

void Thread_pause(Thread self)
{
	if (self == NULL) return;
	if (GetCurrentThreadId() == GetThreadId(self->handle)) return;
	Semaphore_wait(self->sem1);
	Semaphore_wait(self->sem2);
	Signal_raise(self->ps);
	Semaphore_wait(self->sem2); // waiting for thread
	Semaphore_post(self->sem2); // free resource
}

void Thread_resume(Thread self)
{
	if (self == NULL) return;
	Semaphore_post(self->sem1);
}

unidesc Thread_getNativeDescriptor(Thread self)
{
	unidesc ret;
	ret.u64 = (self != NULL)? (uint64_t)((size_t)self->handle) : 0;
	return ret;
}

Signal Thread_getCancelSignal(Thread self)
{
	if (self == NULL) return NULL;
	return self->cs;
}

Signal Thread_getPauseSignal(Thread self)
{
	if (self == NULL) return NULL;
	return self->ps;
}

void Thread_setName(Thread self, const char *name)
{
	if (self == NULL || name == NULL) return;
	SetThreadDescription(self->handle, (PCWSTR)name);
}

#define EVENT_DSIZE	1

struct sSignal {
	int unused;
};

Signal Signal_create(void)
{
	char buf[MAX_PATH];
	DgramSocket s = NULL;
	int attempts = 100;
	while (attempts--) {
		if (GetTempFileNameA((LPCSTR)"\\.", (LPCSTR)"sig", 0, (LPSTR)buf) == 0) return NULL;
		LocalDgramSocket_unlinkAddress(buf);
		s = LocalDgramSocket_create(buf);
		if (s) {
			DgramSocket_setRemote(s, (DgramSocketAddress)buf);
			return (Signal)s;
		}
		Thread_sleep(1);
	}
	return NULL;
}

void Signal_raise(Signal self)
{
	if (self == NULL) return;
	DgramSocket s = (DgramSocket)self;
	uint8_t buf[1];
	DgramSocket_write(s, buf, EVENT_DSIZE);
}

void Signal_end(Signal self)
{
	if (self == NULL) return;
	DgramSocket s = (DgramSocket)self;
	uint8_t buf[1];
	while (DgramSocket_read(s, buf, EVENT_DSIZE));
}

bool Signal_event(Signal self)
{
	if (self == NULL) return false;
	DgramSocket s = (DgramSocket)self;
	return (DgramSocket_readAvailable(s, true) > 0);
}

void Signal_destroy(Signal self)
{
	if (self == NULL) return;
	DgramSocket s = (DgramSocket)self;
	DgramSocket_destroy(s);
}

unidesc Signal_getDescriptor(Signal self)
{
	if (self) {
		DgramSocket s = (DgramSocket)self;
		return DgramSocket_getDescriptor(s);
	}
	return Hal_getInvalidUnidesc();
}

#endif // _WIN32 || _WIN64