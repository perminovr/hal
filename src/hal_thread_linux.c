
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
	int state;
	bool autodestroy;
	Signal cs;
	Signal ps;
	Semaphore sem1;
	Semaphore sem2;
};


Semaphore Semaphore_create(int initialValue)
{
	Semaphore self = calloc(1, sizeof(sem_t));
	sem_init((sem_t*) self, 0, initialValue);
	return self;
}

void Semaphore_wait(Semaphore self)
{
	if (self == NULL) return;
	sem_wait((sem_t*) self);
}

void Semaphore_post(Semaphore self)
{
	if (self == NULL) return;
	sem_post((sem_t*) self);
}

void Semaphore_destroy(Semaphore self)
{
	if (self == NULL) return;
	sem_destroy((sem_t*) self);
	free(self);
}


Mutex Mutex_create(void)
{
	Mutex self = calloc(1, sizeof(pthread_mutex_t));
	pthread_mutexattr_t attr;
	bzero(&attr, sizeof(pthread_mutexattr_t));
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init((pthread_mutex_t*)self, &attr);
	return self;
}

void Mutex_lock(Mutex self)
{
	if (self == NULL) return;
	pthread_mutex_lock((pthread_mutex_t*)self);
}

int Mutex_trylock(Mutex self)
{
	if (self == NULL) return -1;
	return pthread_mutex_trylock((pthread_mutex_t*)self);
}

int Mutex_timedlock(Mutex self, int millies)
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

void Mutex_unlock(Mutex self)
{
	if (self == NULL) return;
	pthread_mutex_unlock((pthread_mutex_t*)self);
}

void Mutex_destroy(Mutex self)
{
	if (self == NULL) return;
	pthread_mutex_destroy((pthread_mutex_t*)self);
	free(self);
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

	self->stackSize = stackSize;
	self->parameter = parameter;
	self->function = function;
	self->state = 0;
	self->autodestroy = autodestroy;

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

static void Thread_destroyMem(Thread self)
{
	Semaphore_destroy(self->sem2);
	Semaphore_destroy(self->sem1);
	Signal_destroy(self->ps);
	Signal_destroy(self->cs);
	free(self);
}

static void *destroyAutomaticThread(void *parameter)
{
	Thread self = (Thread)parameter;
	self->function(self->parameter);
	self->state = 0;
	Thread_destroyMem(self);
	pthread_exit(NULL);
}

void Thread_start(Thread self)
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

	self->state = 1;
}

void Thread_destroy(Thread self)
{
	if (self == NULL) return;
	if (self->autodestroy) {
		Signal_raise(self->cs);
		return; // can't do anything else
	}
	if (self->state == 1) {
		pthread_join(self->pthread, NULL);
	}
	Thread_destroyMem(self);
}

void Thread_sleep(int millies)
{
	usleep(millies * 1000);
}

void Thread_yield(void)
{
	sched_yield();
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
		pthread_exit(NULL);
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
	pthread_t pthread = pthread_self();
	if (self->pthread == pthread) return;
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
	ret.u64 = (self != NULL)? self->pthread : 0;
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
	if (self->pthread)
		pthread_setname_np(self->pthread, name);
}


#define EVENT_DSIZE	8

struct sSignal {
	int fd;
};

Signal Signal_create(void)
{
	Signal self = (Signal)calloc(1, sizeof(struct sSignal));
	self->fd = eventfd(0, 0);
	if (self->fd < 0) { free(self); return NULL; }
	fcntl(self->fd, F_SETFL, (fcntl(self->fd, F_GETFL) | O_NONBLOCK));
	return self;
}

void Signal_raise(Signal self)
{
	if (self == NULL) return;
	uint64_t buf = 1;
	(void)write(self->fd, &buf, EVENT_DSIZE);
}

void Signal_end(Signal self)
{
	if (self == NULL) return;
	uint64_t buf = 1;
	(void)read(self->fd, &buf, EVENT_DSIZE);
}

bool Signal_event(Signal self)
{
	if (self == NULL) return false;
	struct pollfd pfd;
	pfd.fd = self->fd;
	pfd.events = POLLIN;
	int rc = poll(&pfd, 1, 0);
	return (rc > 0);
}

void Signal_destroy(Signal self)
{
	if (self == NULL) return;
	close(self->fd);
	free(self);
}

unidesc Signal_getDescriptor(Signal self)
{
	if (self) {
		unidesc ret;
		ret.i32 = self->fd;
		return ret;
	}
	return Hal_getInvalidUnidesc();
}

#endif // __linux__