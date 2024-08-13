
#ifdef __linux__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <semaphore.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include "hal_thread.h"


struct sThread {
	size_t stackSize;
	ThreadExecutionFunction function;
	void *parameter;
	pthread_t pthread;
	bool running;
	bool autodestroy;
    int pauseCnt;
	Signal cs; // cancel signal
	Signal ps; // pause signal
	Signal rs; // resume signal
	Signal us; // user signal
	Semaphore ps_sem; // pause semaphore
};


Semaphore HalSemaphore_create(int initialValue)
{
	Semaphore self = calloc(1, sizeof(sem_t));
	sem_init((sem_t*) self, 0, initialValue);
	return self;
}

void HalSemaphore_wait(Semaphore self)
{
	if (self == NULL) return;
	sem_wait((sem_t*) self);
}

void HalSemaphore_post(Semaphore self)
{
	if (self == NULL) return;
	sem_post((sem_t*) self);
}

void HalSemaphore_destroy(Semaphore self)
{
	if (self == NULL) return;
	sem_destroy((sem_t*) self);
	free(self);
}


Mutex HalMutex_create(void)
{
	Mutex self = calloc(1, sizeof(pthread_mutex_t));
	pthread_mutexattr_t attr;
	memset(&attr, 0, sizeof(pthread_mutexattr_t));
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init((pthread_mutex_t*)self, &attr);
	return self;
}

void HalMutex_lock(Mutex self)
{
	if (self == NULL) return;
	pthread_mutex_lock((pthread_mutex_t*)self);
}

int HalMutex_trylock(Mutex self)
{
	if (self == NULL) return -1;
	return pthread_mutex_trylock((pthread_mutex_t*)self);
}

int HalMutex_timedlock(Mutex self, int millies)
{
	if (self == NULL) return -1;
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	t.tv_sec += millies/1000;
	t.tv_nsec += (millies%1000)*1000*1000;
	if (t.tv_nsec > 1000000000) {
		t.tv_sec++;
		t.tv_nsec -= 1000000000;
	}
	return pthread_mutex_timedlock((pthread_mutex_t*)self, &t);
}

void HalMutex_unlock(Mutex self)
{
	if (self == NULL) return;
	pthread_mutex_unlock((pthread_mutex_t*)self);
}

void HalMutex_destroy(Mutex self)
{
	if (self == NULL) return;
	pthread_mutex_destroy((pthread_mutex_t*)self);
	free(self);
}


RwLock HalRwLock_create(void)
{
	RwLock self = calloc(1, sizeof(pthread_rwlock_t));
	// pthread_rwlockattr_t attr;
	// memset(&attr, 0, sizeof(pthread_rwlockattr_t));
	pthread_rwlock_init((pthread_rwlock_t*)self, NULL /*&attr*/);
	return self;
}

void HalRwLock_wlock(RwLock self)
{
	if (self == NULL) return;
	pthread_rwlock_wrlock((pthread_rwlock_t*)self);
}

void HalRwLock_rlock(RwLock self)
{
	if (self == NULL) return;
	pthread_rwlock_rdlock((pthread_rwlock_t*)self);
}

void HalRwLock_unlock(RwLock self)
{
	if (self == NULL) return;
	pthread_rwlock_unlock((pthread_rwlock_t*)self);
}

void HalRwLock_destroy(RwLock self)
{
	if (self == NULL) return;
	pthread_rwlock_destroy((pthread_rwlock_t*)self);
	free(self);
}


Thread HalThread_create(size_t stackSize, ThreadExecutionFunction function, void *parameter, bool autodestroy)
{
	Thread self = (Thread) calloc(1, sizeof(struct sThread));
	if (!self) return NULL;

	self->cs = HalSignal_create();
	if (!self->cs) {
		goto exit_self;
	}

	self->ps = HalSignal_create();
	if (!self->ps) {
		goto exit_cs;
	}

	self->rs = HalSignal_create();
	if (!self->rs) {
		goto exit_ps;
	}

	self->us = HalSignal_create();
	if (!self->us) {
		goto exit_rs;
	}

	self->ps_sem = HalSemaphore_create(1);
	if (!self->ps_sem) {
		goto exit_us;
	}

	self->stackSize = stackSize;
	self->parameter = parameter;
	self->function = function;
	self->running = false;
	self->pauseCnt = 0;
	self->autodestroy = autodestroy;

	return self;

exit_us:
	HalSignal_destroy(self->us);
exit_rs:
	HalSignal_destroy(self->rs);
exit_ps:
	HalSignal_destroy(self->ps);
exit_cs:
	HalSignal_destroy(self->cs);
exit_self:
	free(self);
	return NULL;
}

static void HalThread_destroyMem(Thread self)
{
	HalSemaphore_destroy(self->ps_sem);
	HalSignal_destroy(self->us);
	HalSignal_destroy(self->rs);
	HalSignal_destroy(self->ps);
	HalSignal_destroy(self->cs);
	free(self);
}

static void *destroyAutomaticThread(void *parameter)
{
	Thread self = (Thread)parameter;
	self->function(self->parameter);
	self->running = false;
	HalThread_destroyMem(self);
	pthread_exit(NULL);
}

void HalThread_start(Thread self)
{
	if (self == NULL) return;
	pthread_attr_t attr;
	pthread_attr_t *pattr = 0;
	if (self->stackSize > 0) {
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, self->stackSize);
		pattr = &attr;
	}
	if (self->autodestroy == true) {
		pthread_create(&self->pthread, pattr, destroyAutomaticThread, self);
		pthread_detach(self->pthread);
	} else {
		pthread_create(&self->pthread, pattr, self->function, self->parameter);
	}

	self->running = true;
}

void HalThread_join(Thread self)
{
	if (self == NULL) return;
	if (!self->autodestroy && self->running) {
		pthread_join(self->pthread, NULL);
	}
}

void HalThread_destroy(Thread self)
{
	if (self == NULL) return;
	if (self->autodestroy) {
		HalSignal_raise(self->cs);
		return; // can't do anything else
	}
	if (self->running) {
		pthread_join(self->pthread, NULL);
		self->running = false;
	}
	HalThread_destroyMem(self);
}

void HalThread_sleep(int millies)
{
	usleep(millies * 1000);
}

void HalThread_yield(void)
{
	usleep(1);
}

void HalThread_cancel(Thread self)
{
	if (self == NULL) return;
	HalSignal_raise(self->cs);
}

void HalThread_testCancel(Thread self)
{
	if (self == NULL) return;
	if (HalSignal_event(self->cs)) {
		HalSignal_end(self->cs);
		if (self->autodestroy)
			return;
		pthread_exit(NULL);
	}
}

void HalThread_testPause(Thread self)
{
	if (self == NULL) return;
	if (HalSignal_event(self->ps)) {
		HalSignal_end(self->ps);
		HalSignal_end(self->rs); // clear for the case
		HalSignal_raise(self->us); // msg to user: on pause
		HalSignal_wait(self->rs); // waiting for 'resume'
		HalSignal_raise(self->us); // msg to user: resume
	}
}

static void HalThread_msgToThread(Thread self, Signal s)
{
	HalSignal_end(self->us); // clear for the case
	HalSignal_raise(s); // msg to thread: pause/resume
	HalSignal_wait(self->us); // waiting for thread
}

void HalThread_pause(Thread self)
{
	if (self == NULL) return;
	if (self->pthread == pthread_self()) return;
	HalSemaphore_wait(self->ps_sem); // lock 3rd+ thread
    if (self->pauseCnt++ == 0) {
	    HalThread_msgToThread(self, self->ps);
    }
	HalSemaphore_post(self->ps_sem); // unlock 3rd+ thread
}

void HalThread_resume(Thread self)
{
	if (self == NULL) return;
	if (self->pthread == pthread_self()) return;
	HalSemaphore_wait(self->ps_sem); // lock 3rd+ thread
    if (--self->pauseCnt < 0) { self->pauseCnt = 0; }
    if (self->pauseCnt == 0) {
	    HalThread_msgToThread(self, self->rs);
    }
	HalSemaphore_post(self->ps_sem); // unlock 3rd+ thread
}

unidesc HalThread_getNativeDescriptor(Thread self)
{
	unidesc ret;
	ret.u64 = (self != NULL)? self->pthread : 0;
	return ret;
}

unidesc HalThread_getCurrentThreadNativeDescriptor(void)
{
	unidesc ret;
	ret.u64 = (uint64_t)pthread_self();
	return ret;
}

Signal HalThread_getCancelSignal(Thread self)
{
	if (self == NULL) return NULL;
	return self->cs;
}

Signal HalThread_getPauseSignal(Thread self)
{
	if (self == NULL) return NULL;
	return self->ps;
}

void HalThread_setName(Thread self, const char *name)
{
	if (self == NULL || name == NULL) return;
	if (self->pthread)
		pthread_setname_np(self->pthread, name);
}


#define EVENT_DSIZE	8

struct sSignal {
	int fd;
};

Signal HalSignal_create(void)
{
	Signal self = (Signal)calloc(1, sizeof(struct sSignal));
	self->fd = eventfd(0, 0);
	if (self->fd < 0) { free(self); return NULL; }
	fcntl(self->fd, F_SETFL, (fcntl(self->fd, F_GETFL) | O_NONBLOCK));
	return self;
}

void HalSignal_raise(Signal self)
{
	if (self == NULL) return;
	uint64_t buf = 1;
	int rc = write(self->fd, &buf, EVENT_DSIZE);
	(void)rc;
}

void HalSignal_end(Signal self)
{
	if (self == NULL) return;
	uint64_t buf = 1;
	int rc = read(self->fd, &buf, EVENT_DSIZE);
	(void)rc;
}

bool HalSignal_event(Signal self)
{
	if (self == NULL) return false;
	struct pollfd pfd;
	pfd.fd = self->fd;
	pfd.events = POLLIN;
	int rc = poll(&pfd, 1, 0);
	return (rc > 0 && (pfd.revents&POLLIN));
}

void HalSignal_wait(Signal self)
{
	if (self == NULL) return;
	int rc;
	struct pollfd pfd;
	pfd.fd = self->fd;
	pfd.events = POLLIN;
	do {
		rc = poll(&pfd, 1, -1);
	} while (rc <= 0 || (pfd.revents & POLLIN) == 0);
	HalSignal_end(self);
}

void HalSignal_destroy(Signal self)
{
	if (self == NULL) return;
	close(self->fd);
	free(self);
}

unidesc HalSignal_getDescriptor(Signal self)
{
	if (self) {
		unidesc ret;
		ret.i32 = self->fd;
		return ret;
	}
	return Hal_getInvalidUnidesc();
}

#endif // __linux__