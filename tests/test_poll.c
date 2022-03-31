
#include <stdio.h>
#include "hal_poll.h"
#include "hal_time.h"
#include "hal_thread.h"
#include "hal_timer.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
    int rc, revents;
    Signal s = Signal_create();
    Timer t = Timer_create();
    struct sPollfd pfd[HAL_POLL_MAX];
    memset(pfd, 0, sizeof(pfd));
    for (int i = 0; i < HAL_POLL_MAX; ++i) {
        pfd[i].fd = Hal_getInvalidUnidesc();
    }
    pfd[0].events = HAL_POLLIN;
    pfd[0].fd = Signal_getDescriptor(s);
    pfd[1].events = HAL_POLLIN;
    pfd[1].fd = Timer_getDescriptor(t);
    uint64_t ts0 = Hal_getTimeInMs();
    switch (test) {
        case 1: { // timeout
            rc = Hal_poll(pfd, 2, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc != 0) { err(); return 1; }
            if (ts < 480 || ts > 520) { err(); return 1; }
            return 0;
        } break;
        case 2: { // event
            AccurateTime_t at; at.sec = 0; at.nsec = 200 * 1000 * 1000;
            Timer_setTimeout(t, &at);
            rc = Hal_poll(pfd, HAL_POLL_MAX, 1000);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts < 180 || ts > 220) { err(); return 1; }
            if ( (pfd[1].revents&HAL_POLLIN) == 0) { err(); return 1; }
            return 0;
        } break;
        case 3: { // invalid descr
            AccurateTime_t at; at.sec = 0; at.nsec = 100 * 1000 * 1000;
            Timer_setTimeout(t, &at);
            pfd[0].fd = Hal_getInvalidUnidesc();
            pfd[0].events = 0;
            rc = Hal_poll(pfd, HAL_POLL_MAX, 1000);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts < 80 || ts > 120) { err(); return 1; }
            if ( (pfd[1].revents&HAL_POLLIN) == 0) { err(); return 1; }
            return 0;
        } break;
        case 4: { // pollout
            pfd[0].events = HAL_POLLIN|HAL_POLLOUT;
            rc = Hal_poll(pfd, HAL_POLL_MAX, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts > 20) { err(); return 1; }
            if ( (pfd[0].revents&HAL_POLLOUT) == 0) { err(); return 1; }
            return 0;
        } break;
        case 5: { // single timeout
            rc = Hal_pollSingle(Timer_getDescriptor(t), HAL_POLLIN, NULL, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc != 0) { err(); return 1; }
            if (ts < 480 || ts > 520) { err(); return 1; }
            return 0;
        } break;
        case 6: { // single event
            AccurateTime_t at; at.sec = 0; at.nsec = 200 * 1000 * 1000;
            Timer_setTimeout(t, &at);
            rc = Hal_pollSingle(Timer_getDescriptor(t), HAL_POLLIN, &revents, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts < 180 || ts > 220) { err(); return 1; }
            if ( (revents&HAL_POLLIN) == 0) { err(); return 1; }
            return 0;
        } break;
        case 7: { // single pollout
            rc = Hal_pollSingle(Signal_getDescriptor(s), HAL_POLLIN|HAL_POLLOUT, &revents, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts > 20) { err(); return 1; }
            if ( (revents&HAL_POLLOUT) == 0) { err(); return 1; }
            return 0;
        } break;
        case 8: { // pollinout
            Signal_raise(s);
            pfd[0].events = HAL_POLLIN|HAL_POLLOUT;
            rc = Hal_poll(pfd, HAL_POLL_MAX, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts > 20) { err(); return 1; }
            if ( (pfd[0].revents&HAL_POLLOUT) == 0) { err(); return 1; }
            if ( (pfd[0].revents&HAL_POLLIN) == 0) { err(); return 1; }
            return 0;
        } break;
        case 9: { // single pollinout
            Signal_raise(s);
            rc = Hal_pollSingle(Signal_getDescriptor(s), HAL_POLLIN|HAL_POLLOUT, &revents, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts > 20) { err(); return 1; }
            if ( (revents&HAL_POLLOUT) == 0) { err(); return 1; }
            if ( (revents&HAL_POLLIN) == 0) { err(); return 1; }
            return 0;
        } break;
    }

    { err(); return 1; }
}