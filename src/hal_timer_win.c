
#if defined(_WIN32) || defined(_WIN64)

#include <time.h>
#include <windows.h>
#include "hal_timer.h"
#include "hal_thread.h"


struct sTimer {
	Signal sig;
	UINT uTimerID;
	bool periodical;
	UINT lastTimeout;
};


static void Timer_callback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	(void)uMsg;
	(void)dw1;
	(void)dw2;
	Timer self = (Timer)dwUser;
	if (!self->periodical) {
		timeKillEvent(uTimerID);
		self->uTimerID = 0;
	}
	Signal_raise(self->sig);
}

static bool Timer_set(Timer self, UINT delay, bool periodically)
{
	if (self->uTimerID) timeKillEvent(self->uTimerID);
	self->uTimerID = timeSetEvent(delay, 0, 
			Timer_callback, (DWORD_PTR)self, 
			((periodically)? TIME_PERIODIC : TIME_ONESHOT));
	if (self->uTimerID) {
		self->periodical = periodically;
		return true;
	}
	return false;
}

static inline UINT Timer_getDelay(AccurateTime_t *timeout)
{
	// UINT ms = timeout->nsec/1000000;
	// if ((timeout->nsec - ms*1000000) >= 5*100000) ms++;
	return timeout->sec * 1000 + timeout->nsec/1000000;
}


bool Timer_setTimeout(Timer self, AccurateTime_t *timeout)
{
	if (self == NULL) return false;
	if (timeout == NULL) return false;

	Timer_endEvent(self);

	UINT delay = Timer_getDelay(timeout);
	bool ret = Timer_set(self, delay, false);
	if (ret) { self->lastTimeout = delay; }
	return ret;
}

bool Timer_repeatTimeout(Timer self)
{
	if (self == NULL) return false;
	return Timer_set(self, self->lastTimeout, false);
}

bool Timer_setPeriod(Timer self, AccurateTime_t *period)
{
	if (self == NULL) return false;
	if (period == NULL) return false;

	Timer_endEvent(self);

	UINT delay = Timer_getDelay(period);
	return Timer_set(self, delay, true);
}

bool Timer_stop(Timer self)
{
	if (self == NULL) return false;
	if (self->uTimerID) timeKillEvent(self->uTimerID);
	self->uTimerID = 0;
	Timer_endEvent(self);
	return true;
}

void Timer_endEvent(Timer self)
{
	if (self == NULL) return;
	Signal_end(self->sig);
}

Timer Timer_create(void)
{
	Signal sig = Signal_create();
	if (!sig) return NULL;
	Timer self = (Timer)calloc(1, sizeof(struct sTimer));
	if (self) {
		self->sig = sig;
	} else {
		Signal_destroy(sig);
	}
	return self;
}

void Timer_destroy(Timer self)
{
	if (self == NULL) return;
	if (self->uTimerID) timeKillEvent(self->uTimerID);
	Signal_destroy(self->sig);
	free(self);
}

unidesc Timer_getDescriptor(Timer self)
{
	if (self) {
		return Signal_getDescriptor(self->sig);
	}
	return Hal_getInvalidUnidesc();
}


typedef struct sTimerSingleShotEntry * TimerSingleShotEntry;
struct sTimerSingleShotEntry {
	Timer self;
	TimerSingleShotEntry next;
	bool active;
};
typedef struct {
	Mutex mu;
	TimerSingleShotEntry head;
} TimerSingleShotList;
static TimerSingleShotList tssList;

unidesc Timer_setSingleShot(AccurateTime_t *timeout)
{
	TimerSingleShotEntry tsse;
	unidesc ret = Hal_getInvalidUnidesc();

	// init mutex
	if (!tssList.mu) { tssList.mu = Mutex_create(); }

	Mutex_lock(tssList.mu);

	// find free entry
	for (tsse = tssList.head; tsse; tsse = tsse->next) {
		if (tsse->active == false) {
			break;
		}
	}

	// new entry
	if (!tsse) {
		TimerSingleShotEntry t;
		tsse = (TimerSingleShotEntry)calloc(1, sizeof(struct sTimerSingleShotEntry));
		if (!tsse) { goto toexit; }
		tsse->self = Timer_create();
		if (!tsse->self) { free(tsse); goto toexit; }
		for (t = tssList.head; t && t->next; t = t->next) {}
		if (t) { t->next = tsse; }
		else { tssList.head = tsse; }
	}
	
	tsse->active = true;
	Timer_setTimeout(tsse->self, timeout);
	ret = Timer_getDescriptor(tsse->self);
	
toexit:
	Mutex_unlock(tssList.mu);
	return ret;
}

void Timer_endSingleShot(unidesc desc)
{
	Mutex_lock(tssList.mu);
	for (TimerSingleShotEntry tsse = tssList.head; tsse; tsse = tsse->next) {
		unidesc fd = Timer_getDescriptor(tsse->self);
		if ( Hal_unidescIsEqual(&fd, &desc) ) {
			Timer_endEvent(tsse->self);
			tsse->active = false;
			break;
		}
	}
	Mutex_unlock(tssList.mu);
}

#endif // _WIN32 || _WIN64