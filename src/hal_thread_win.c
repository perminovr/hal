
#if defined(_WIN32) || defined(_WIN64)

#include "hal_thread.h"
#include "hal_syshelper.h"
#include <winsock2.h>
#include <windows.h>


struct sThread {
	ThreadExecutionFunction function;
	void *parameter;
	HANDLE handle;
	DWORD threadId;
	int state;
	bool autodestroy;
	Signal killSignal;
	Signal cancelSignal;
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


static DWORD WINAPI destroyAutomaticThreadRunner(LPVOID parameter)
{
	Thread self = (Thread) parameter;
	self->function(self->parameter);
	self->state = 0;
	Thread_destroy(self);
	return 0;
}

static DWORD WINAPI threadRunner(LPVOID parameter)
{
	Thread self = (Thread) parameter;
	return (DWORD)self->function(self->parameter);
}

Thread Thread_create(size_t stackSize, ThreadExecutionFunction function, void *parameter, bool autodestroy)
{
	Thread thread = (Thread) calloc(1, sizeof(struct sThread));

	if (thread != NULL) {
		thread->cancelSignal = Signal_create();
		if (thread->cancelSignal) {
			thread->parameter = parameter;
			thread->function = function;
			thread->state = 0;
			thread->autodestroy = autodestroy;
			thread->handle = CreateThread(
					0, (SIZE_T)stackSize, 
					((autodestroy)? destroyAutomaticThreadRunner : threadRunner), 
					(LPVOID)thread, CREATE_SUSPENDED, (LPDWORD)&thread->threadId
			);
		}
	}

	return thread;
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
	if (self->state == 1) {
		WaitForSingleObject(self->handle, INFINITE);
	}
	CloseHandle(self->handle);
	free(self);
}

void Thread_sleep(int millies)
{
	Sleep(millies);
}

void Thread_yield(void)
{
	SwitchToThread();
}

void Thread_cancel(Thread self)
{
	if (self == NULL) return;
	if (self->killSignal)
		Signal_raise(self->killSignal);
	if (!self->autodestroy)
		Signal_raise(self->cancelSignal);
}

void Thread_testCancel(Thread self)
{
	if (self == NULL) return;
	if (self->killSignal)
		Signal_end(self->killSignal);
	if (!self->autodestroy) {
		if (Signal_event(self->cancelSignal)) {
			Signal_end(self->cancelSignal);
			ExitThread(0);
		}
	}
}

unidesc Thread_getNativeDescriptor(Thread self)
{
	unidesc ret;
	ret.u64 = (self != NULL)? (uint64_t)((size_t)self->handle) : 0;
	return ret;
}

void Thread_setCancelSignal(Thread self, Signal signal)
{
	if (self == NULL) return;
	self->killSignal = signal;
}


#define EVENT_DSIZE	1

struct sSignal {
	int unused;
};

Signal Signal_create(void)
{
	return (Signal)HalShFdesc_create(Hal_getInvalidUnidesc());
}

void Signal_raise(Signal self)
{
	if (self == NULL) return;
	HalShFdesc fd = (HalShFdesc)self;
	uint8_t buf[1];
	DgramSocket_write(fd->i, buf, EVENT_DSIZE);
}

void Signal_end(Signal self)
{
	if (self == NULL) return;
	HalShFdesc fd = (HalShFdesc)self;
	uint8_t buf[1];
	while (DgramSocket_read(fd->e, buf, EVENT_DSIZE));
}

bool Signal_event(Signal self)
{
	if (self == NULL) return false;
	HalShFdesc fd = (HalShFdesc)self;
	return (DgramSocket_readAvailable(fd->e) > 0);
}

void Signal_destroy(Signal self)
{
	if (self == NULL) return;
	HalShFdesc fd = (HalShFdesc)self;
	HalShFdesc_destroy(fd);
}

unidesc Signal_getDescriptor(Signal self)
{
	if (self) {
		HalShFdesc fd = (HalShFdesc)self;
		return HalShFdesc_getDescriptor(fd);
	}
	return Hal_getInvalidUnidesc();
}

#endif // _WIN32 || _WIN64