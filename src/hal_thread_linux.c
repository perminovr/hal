
#ifdef __linux__

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
	Signal killSignal;
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
	t.tv_sec = millies/1000;
	t.tv_nsec = (millies%1000)*1000*1000;
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
	Thread thread = (Thread) calloc(1, sizeof(struct sThread));

	if (thread != NULL) {
		thread->stackSize = stackSize;
		thread->parameter = parameter;
		thread->function = function;
		thread->state = 0;
		thread->autodestroy = autodestroy;
	}

	return thread;
}

static void *destroyAutomaticThread(void *parameter)
{
	Thread thread = (Thread)parameter;
	thread->function(thread->parameter);
	free(thread);
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
	if (self->state == 1) {
		pthread_join(self->pthread, NULL);
	}
	free(self);
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
	if (self->killSignal)
		Signal_raise(self->killSignal);
	if (!self->autodestroy)
		pthread_cancel(self->pthread);
}

void Thread_testCancel(Thread self)
{
	if (self == NULL) return;
	if (self->killSignal)
		Signal_end(self->killSignal);
	if (!self->autodestroy)
		pthread_testcancel();
}

unidesc Thread_getNativeDescriptor(Thread thread)
{
	unidesc ret;
	ret.u64 = (thread != NULL)? thread->pthread : 0;
	return ret;
}

void Thread_setCancelSignal(Thread self, Signal signal)
{
	if (self == NULL) return;
	self->killSignal = signal;
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