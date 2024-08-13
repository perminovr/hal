
#ifdef __linux__

#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include "hal_timer.h"


struct sTimer {
	int fd;
	bool running;
	AccurateTime_t lastTimeout;
};


bool Timer_setTimeout(Timer self, AccurateTime_t *timeout)
{
	if (self == NULL) return false;
	if (timeout == NULL) return false;

	Timer_endEvent(self);

	struct itimerspec its;
	memset(&its, 0, sizeof(struct itimerspec));

	its.it_value.tv_sec = timeout->sec;
	its.it_value.tv_nsec = timeout->nsec;

	self->lastTimeout.sec = timeout->sec;
	self->lastTimeout.nsec = timeout->nsec;

	timerfd_settime(self->fd, 0, &its, NULL);
	self->running = true;

	return true;
}

bool Timer_repeatTimeout(Timer self)
{
	if (self == NULL) return false;

	struct itimerspec its;
	memset(&its, 0, sizeof(struct itimerspec));

	its.it_value.tv_sec = self->lastTimeout.sec;
	its.it_value.tv_nsec = self->lastTimeout.nsec;

	timerfd_settime(self->fd, 0, &its, NULL);
	self->running = true;

	return true;
}

bool Timer_setPeriod(Timer self, AccurateTime_t *period)
{
	if (self == NULL) return false;
	if (period == NULL) return false;

	Timer_endEvent(self);

	struct itimerspec its;
	memset(&its, 0, sizeof(struct itimerspec));

	its.it_value.tv_sec = period->sec;
	its.it_value.tv_nsec = period->nsec;
	its.it_interval.tv_sec = period->sec;
	its.it_interval.tv_nsec = period->nsec;

	timerfd_settime(self->fd, 0, &its, NULL);
	self->running = true;

	return true;
}

bool Timer_stop(Timer self)
{
	if (self == NULL) return false;
	struct itimerspec its;
	memset(&its, 0, sizeof(struct itimerspec));
	timerfd_settime(self->fd, 0, &its, NULL);
	Timer_endEvent(self);
	return true;
}

void Timer_endEvent(Timer self)
{
	if (self == NULL) return;
	uint64_t buf;
	int rc = read(self->fd, &buf, sizeof(uint64_t));
	(void)rc;
	self->running = false;
}

Timer Timer_create(void)
{
	Timer self = (Timer)calloc(1, sizeof(struct sTimer));
	if (self) {
		self->fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK);
		if (self->fd < 0) {
			free(self);
			return NULL;
		}
		self->running = false;
	}
	return self;
}

void Timer_destroy(Timer self)
{
	if (self != NULL) {
		close(self->fd);
		free(self);
	}
}

unidesc Timer_getDescriptor(Timer self)
{
	if (self) {
		unidesc ret;
		ret.i32 = self->fd;
		return ret;
	}
	return Hal_getInvalidUnidesc();
}

bool Timer_isRunning(Timer self)
{
	return self? self->running : false;
}

unidesc Timer_setSingleShot(AccurateTime_t *time)
{
	unidesc ud;
	ud.i32 = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK);
	if (ud.i32 < 0) {
		return ud;
	}

	struct itimerspec its;
	memset(&its, 0, sizeof(struct itimerspec));

	its.it_value.tv_sec = time->sec;
	its.it_value.tv_nsec = time->nsec;

	timerfd_settime(ud.i32, 0, &its, NULL);

	return ud;
}

void Timer_endSingleShot(unidesc desc)
{
	close(desc.i32);
}

#endif // __linux__