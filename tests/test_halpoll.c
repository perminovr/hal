
#include <stdio.h>
#include "hal_poll.h"
#include "hal_time.h"
#include "hal_thread.h"
#include "hal_timer.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

static int poll_cb_passed = 0;
static int poll_cb_revents = 0;
void poll_cb(void *user, void *object, int revents)
{
    poll_cb_revents = revents;
    if ( user == ((void *)((size_t)1)) && object == ((void *)((size_t)2)) )
        poll_cb_passed++;
}

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
    int rc;
    Signal s = Signal_create();
    Timer t = Timer_create();
    HalPoll h = HalPoll_create(HAL_POLL_MAX*2);
    void *user = ((void *)((size_t)1));
    void *object = ((void *)((size_t)2));
    HalPoll_update(h, Signal_getDescriptor(s), HAL_POLLIN, object, user, poll_cb);
    HalPoll_update(h, Timer_getDescriptor(t), HAL_POLLIN, object, user, poll_cb);
    uint64_t ts0 = Hal_getTimeInMs();
    switch (test) {
        case 1: { // timeout
            rc = HalPoll_wait(h, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc != 0) { err(); return 1; }
            if (ts < 480 || ts > 520) { err(); return 1; }
            HalPoll_destroy(h);
            return 0;
        } break;
        case 2: { // event
            AccurateTime_t at; at.sec = 0; at.nsec = 200 * 1000 * 1000;
            Timer_setTimeout(t, &at);
            rc = HalPoll_wait(h, 1000);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts < 180 || ts > 220) { err(); return 1; }
            if ( (poll_cb_revents&HAL_POLLIN) == 0) { err(); return 1; }
            if ( poll_cb_passed == 0) { err(); return 1; }
            HalPoll_destroy(h);
            return 0;
        } break;
        case 3: { // disable descr
            AccurateTime_t at; at.sec = 0; at.nsec = 100 * 1000 * 1000;
            Timer_setTimeout(t, &at);
            HalPoll_update_1(h, Signal_getDescriptor(s), 0);
            rc = HalPoll_wait(h, 1000);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts < 80 || ts > 120) { err(); return 1; }
            if ( (poll_cb_revents&HAL_POLLIN) == 0) { err(); return 1; }
            if ( poll_cb_passed == 0) { err(); return 1; }
            HalPoll_destroy(h);
            return 0;
        } break;
        case 4: { // pollout
            HalPoll_update_1(h, Signal_getDescriptor(s), HAL_POLLIN|HAL_POLLOUT);
            rc = HalPoll_wait(h, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts > 20) { err(); return 1; }
            if ( (poll_cb_revents&HAL_POLLOUT) == 0) { err(); return 1; }
            if ( poll_cb_passed == 0) { err(); return 1; }
            HalPoll_destroy(h);
            return 0;
        } break;
        case 5: { // pollinout
            Signal_raise(s);
            HalPoll_update_1(h, Signal_getDescriptor(s), HAL_POLLIN|HAL_POLLOUT);
            rc = HalPoll_wait(h, 500);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts > 20) { err(); return 1; }
            if ( (poll_cb_revents&HAL_POLLOUT) == 0) { err(); return 1; }
            if ( (poll_cb_revents&HAL_POLLIN) == 0) { err(); return 1; }
            if ( poll_cb_passed == 0) { err(); return 1; }
            HalPoll_destroy(h);
            return 0;
        } break;
        case 6: { // remove
            AccurateTime_t at; at.sec = 0; at.nsec = 100 * 1000 * 1000;
            Timer_setTimeout(t, &at);
            HalPoll_remove(h, Signal_getDescriptor(s));
            rc = HalPoll_wait(h, 1000);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts < 80 || ts > 120) { err(); return 1; }
            if ( (poll_cb_revents&HAL_POLLIN) == 0) { err(); return 1; }
            if ( poll_cb_passed == 0) { err(); return 1; }
            HalPoll_destroy(h);
            return 0;
        } break;
        case 7: { // updates
            Timer t2 = Timer_create();
            unidesc ud1 = Signal_getDescriptor(s);
            unidesc ud2 = Timer_getDescriptor(t);
            unidesc ud3 = Timer_getDescriptor(t2);
            HalPoll_remove(h, ud1);
            HalPoll_remove(h, ud2);
            HalPoll_update(h, ud1, HAL_POLLIN, NULL, NULL, NULL);
            HalPoll_update(h, ud2, HAL_POLLIN, NULL, NULL, NULL);
            HalPoll_update_2(h, ud1, object);
            HalPoll_update_2(h, ud2, object);
            HalPoll_update_3(h, ud1, user, poll_cb);
            HalPoll_update_3(h, ud2, user, poll_cb);
            HalPoll_update_4(h, ud1, ud3);
            //
            AccurateTime_t at; at.sec = 0; at.nsec = 200 * 1000 * 1000;
            Timer_setTimeout(t2, &at);
            rc = HalPoll_wait(h, 1000);
            uint64_t ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) { err(); return 1; }
            if (ts < 180 || ts > 220) { err(); return 1; }
            if ( (poll_cb_revents&HAL_POLLIN) == 0) { err(); return 1; }
            if ( poll_cb_passed == 0) { err(); return 1; }
            HalPoll_destroy(h);
            return 0;
        } break;
        case 8: {
			AccurateTime_t at;
			uint64_t ts;
			HalPoll_destroy(h);
			h = HalPoll_create(1);
			HalPoll_update(h, Timer_getDescriptor(t), HAL_POLLIN, object, user, poll_cb);
			//
			at.sec = 0; at.nsec = 200 * 1000 * 1000;
			Timer_setTimeout(t, &at);
			//
			Timer t2 = Timer_create();
			at.sec = 0; at.nsec = 100 * 1000 * 1000;
			Timer_setTimeout(t2, &at);
			if (!HalPoll_resize(h, 2)) { err(); return 1; }
			if (!HalPoll_update(h, Timer_getDescriptor(t2), HAL_POLLIN, object, user, poll_cb)) { err(); return 1; }
			//
			ts0 = Hal_getTimeInMs();
			rc = HalPoll_wait(h, 500); // t2
			ts = Hal_getTimeInMs() - ts0;
			Timer_endEvent(t2);
			if (rc <= 0) { err(); return 1; }
			if (ts > 120 || ts < 80) { err(); return 1; }
			//
			ts0 = Hal_getTimeInMs();
			rc = HalPoll_wait(h, 500); // t
			ts = Hal_getTimeInMs() - ts0;
			Timer_endEvent(t);
			if (rc <= 0) { err(); return 1; }
			if (ts > 120 || ts < 80) { err(); return 1; }
			//
			HalPoll_destroy(h);
			return 0;
		} break;
    }

    { err(); return 1; }
}