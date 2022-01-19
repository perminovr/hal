#include "hal_base.h"

#ifndef HAL_NOT_EMPTY

#include "hal_thread.h"


struct sThread {
	size_t stackSize;
	ThreadExecutionFunction function;
	void *parameter;
	int state;
	bool autodestroy;
};


Semaphore Semaphore_create(int initialValue)
{
	return NULL;
}

void Semaphore_wait(Semaphore self)
{
}

void Semaphore_post(Semaphore self)
{
}

void Semaphore_destroy(Semaphore self)
{
}


Mutex Mutex_create(void)
{
	return NULL;
}

void Mutex_lock(Mutex self)
{
}

int Mutex_trylock(Mutex self)
{
	return -1;
}

int Mutex_timedlock(Mutex self, int millies)
{
	return -1;
}

void Mutex_unlock(Mutex self)
{
}

void Mutex_destroy(Mutex self)
{
}


Thread Thread_create(size_t stackSize, ThreadExecutionFunction function, void *parameter, bool autodestroy)
{
	return NULL;
}

void Thread_start(Thread self)
{
}

void Thread_destroy(Thread self)
{
}

void Thread_sleep(int millies)
{
}


#endif // HAL_NOT_EMPTY