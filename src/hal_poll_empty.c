#include "hal_base.h"

#ifndef HAL_NOT_EMPTY

#include "hal_poll.h"


int Hal_poll(Pollfd pfd, unsigned long int size, int timeout)
{
	return -1;
}



typedef struct {
	void *object;
	void *user;
	PollfdReventsHandler handler;
} EventObject;

struct sHalPoll {
	EventObject *objects;

	int maxSize;
	int size;
	void *user;
	PollfdReventsHandler *handler;
};


HalPoll HalPoll_create(int maxSize)
{
	return NULL;
}


bool HalPoll_update(HalPoll self, unidesc fd, int events, void *object, void *user, PollfdReventsHandler handler)
{
	return false;
}


bool HalPoll_update_1(HalPoll self, unidesc fd, int events)
{
	return false;
}


bool HalPoll_update_2(HalPoll self, unidesc fd, void *object)
{
	return false;
}


bool HalPoll_update_3(HalPoll self, unidesc fd, void *user, PollfdReventsHandler handler)
{
	return false;
}


bool HalPoll_remove(HalPoll self, unidesc fd)
{
	return false;
}


int HalPoll_wait(HalPoll self, int timeout)
{
	return -1;
}


void HalPoll_destroy(HalPoll self)
{
}


#endif // HAL_NOT_EMPTY