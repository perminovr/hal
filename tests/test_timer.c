
#include <stdio.h>
#include "hal_poll.h"
#include "hal_time.h"
#include "hal_timer.h"
#include "hal_thread.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
	int rc, revents;
	Timer t = Timer_create();
	uint64_t ts, ts0;
	AccurateTime_t at; at.sec = 0; at.nsec = 100 * 1000 * 1000;
	switch (test) {
		case 1: { // timeout, endev, repeat, stop
			// set timeout
			ts0 = Hal_getTimeInMs();
			Timer_setTimeout(t, &at);
			rc = Hal_pollSingle(Timer_getDescriptor(t), HAL_POLLIN, &revents, 500);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			// test end event
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(Timer_getDescriptor(t), HAL_POLLIN, &revents, 500);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts > 20) { err(); return 1; }
			Timer_endEvent(t);
			// repeat
			ts0 = Hal_getTimeInMs();
			Timer_repeatTimeout(t);
			rc = Hal_pollSingle(Timer_getDescriptor(t), HAL_POLLIN, &revents, 500);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			Timer_endEvent(t);
			// repeat and stop
			ts0 = Hal_getTimeInMs();
			Timer_repeatTimeout(t);
			Thread_sleep(50);
			Timer_stop(t);
			rc = Hal_pollSingle(Timer_getDescriptor(t), HAL_POLLIN, &revents, 300);
			ts = Hal_getTimeInMs() - ts0;
			if (rc != 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) != 0) { err(); return 1; }
			if (ts < 330) { err(); return 1; }
			Timer_destroy(t);
			return 0;
		} break;
		case 2: { // period
			Timer_setPeriod(t, &at);
			// 1
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(Timer_getDescriptor(t), HAL_POLLIN, &revents, 500);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			Timer_endEvent(t);
			// 2
			ts0 = Hal_getTimeInMs();
			rc = Hal_pollSingle(Timer_getDescriptor(t), HAL_POLLIN, &revents, 500);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			Timer_destroy(t);
			return 0;
		} break;
		case 3: { // single shot
			struct sPollfd pfd[3];
			at.nsec = 100 * 1000 * 1000;
			pfd[0].fd = Timer_setSingleShot(&at);
			pfd[0].events = HAL_POLLIN;
			at.nsec = 200 * 1000 * 1000;
			pfd[1].fd = Timer_setSingleShot(&at);
			pfd[1].events = HAL_POLLIN;
			at.nsec = 300 * 1000 * 1000;
			pfd[2].fd = Timer_setSingleShot(&at);
			pfd[2].events = HAL_POLLIN;
			// 1
			ts0 = Hal_getTimeInMs();
			rc = Hal_poll(pfd, 3, 500);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (pfd[0].revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			Timer_endSingleShot(pfd[0].fd);
			pfd[0].fd = Hal_getInvalidUnidesc();
			pfd[0].events = 0;
			// 2
			ts0 = Hal_getTimeInMs();
			rc = Hal_poll(pfd, 3, 500);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (pfd[1].revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			Timer_endSingleShot(pfd[1].fd);
			pfd[1].fd = Hal_getInvalidUnidesc();
			pfd[1].events = 0;
			// 3
			ts0 = Hal_getTimeInMs();
			rc = Hal_poll(pfd, 3, 500);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (pfd[2].revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			Timer_endSingleShot(pfd[2].fd);
			pfd[2].fd = Hal_getInvalidUnidesc();
			pfd[2].events = 0;
			// 4
			at.nsec = 100 * 1000 * 1000;
			pfd[0].fd = Timer_setSingleShot(&at);
			pfd[0].events = HAL_POLLIN;
			ts0 = Hal_getTimeInMs();
			rc = Hal_poll(pfd, 3, 500);
			ts = Hal_getTimeInMs() - ts0;
			if (rc <= 0) { err(); return 1; }
			if ( (pfd[0].revents&HAL_POLLIN) == 0) { err(); return 1; }
			if (ts < 80 || ts > 120) { err(); return 1; }
			Timer_endSingleShot(pfd[0].fd);
			return 0;
		} break;
	}

	{ err(); return 1; }
}