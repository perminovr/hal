
#include <stdio.h>
#include "hal_time.h"
#include "hal_poll.h"
#include "hal_thread.h"

typedef struct {
    Thread self;
    Signal cs;
    int n;
    int res;
    int done;
    Semaphore sem;
    Mutex mu;
} test_t;
static test_t tt;

void *thread_loop(void *arg)
{
    int rc;
    if (arg != &tt) tt.res = 1;
    switch (tt.n) {
        case 2: {
            uint64_t ts, ts0;
            ts0 = Hal_getTimeInMs();
            rc = Hal_pollSingle(Signal_getDescriptor(tt.cs), HAL_POLLIN, NULL, -1);
            ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) tt.res = 1;
            if (ts < 80) tt.res = 1;
            Thread_testCancel(tt.self);
            // should exit thread before this point
            tt.res = 1;
        } break;
        case 3: {
            Thread_sleep(100);
            tt.done = 1;
        } break;
        case 4: {
            Thread_sleep(100);
            Semaphore_post(tt.sem);
        } break;
        case 5: {
            Thread_sleep(100);
            Semaphore_post(tt.sem);
        } break;
        case 6: {
            Mutex_lock(tt.mu);
            Thread_sleep(100);
            Mutex_unlock(tt.mu);
        } break;
        case 7: {
            Mutex_lock(tt.mu);
            Thread_sleep(200);
            Mutex_unlock(tt.mu);
        } break;
    }
    return NULL;
}

int main(int argc, const char **argv)
{
    int rc;
    uint64_t ts, ts0;
    memset(&tt, 0, sizeof(tt));
    tt.n = atoi(argv[1]); 
    switch (tt.n) {
        case 1: { // sleep
            ts0 = Hal_getTimeInMs();
            Thread_sleep(100);
            ts = Hal_getTimeInMs() - ts0;
            if (ts < 80) return 1;
            return 0;
        } break;
        case 2: { // cancel
            tt.cs = Signal_create();
            tt.self = Thread_create(65535, thread_loop, &tt, false);
            Thread_setCancelSignal(tt.self, tt.cs);
            Thread_start(tt.self);
            Thread_sleep(100);
            Thread_cancel(tt.self);
            Signal_raise(tt.cs);
            Thread_destroy(tt.self);
            Signal_destroy(tt.cs);
            if (tt.res) return 1;
            return 0;
        } break;
        case 3: { // autodestroy
            tt.self = Thread_create(65535, thread_loop, &tt, true);
            Thread_start(tt.self);
            ts0 = Hal_getTimeInMs();
            while (tt.done == 0) { Thread_sleep(1); }
            ts = Hal_getTimeInMs() - ts0;
            if (ts < 80) return 1;
            return 0;
        } break;
        case 4: { // sem
            tt.sem = Semaphore_create(1);
            tt.self = Thread_create(65535, thread_loop, &tt, true);
            ts0 = Hal_getTimeInMs();
            Semaphore_wait(tt.sem);
            Thread_start(tt.self);
            Semaphore_wait(tt.sem); // self-lock
            Semaphore_post(tt.sem);
            ts = Hal_getTimeInMs() - ts0;
            if (ts < 80 || ts > 120) return 1;
            Semaphore_destroy(tt.sem);
            return 0;
        } break;
        case 5: { // sem2
            tt.sem = Semaphore_create(0); // locked
            tt.self = Thread_create(65535, thread_loop, &tt, true);
            ts0 = Hal_getTimeInMs();
            Thread_start(tt.self);
            Semaphore_wait(tt.sem); // self-lock
            Semaphore_post(tt.sem);
            ts = Hal_getTimeInMs() - ts0;
            if (ts < 80 || ts > 120) return 1;
            Semaphore_destroy(tt.sem);
            return 0;
        } break;
        case 6: { // mutex
            tt.mu = Mutex_create();
            Mutex_lock(tt.mu);
            Mutex_lock(tt.mu);
            Mutex_lock(tt.mu);
            Mutex_unlock(tt.mu);
            Mutex_unlock(tt.mu);
            Mutex_unlock(tt.mu);
            rc = Mutex_trylock(tt.mu);
            if (rc != 0) return 1;
            Mutex_unlock(tt.mu);
            tt.self = Thread_create(65535, thread_loop, &tt, true);
            Thread_start(tt.self);
            Thread_sleep(1);
            ts0 = Hal_getTimeInMs();
            rc = Mutex_trylock(tt.mu); // should be locked
            if (rc == 0) return 1;
            Mutex_lock(tt.mu); // waiting
            ts = Hal_getTimeInMs() - ts0;
            if (ts < 80) return 1;
            Mutex_destroy(tt.mu);
            return 0;
        } break;
        case 7: { // timed mutex
            tt.mu = Mutex_create();
            tt.self = Thread_create(65535, thread_loop, &tt, true);
            Thread_start(tt.self);
            Thread_sleep(1);
            ts0 = Hal_getTimeInMs();
            rc = Mutex_timedlock(tt.mu, 100); // should be locked
            ts = Hal_getTimeInMs() - ts0;
            if (rc == 0) return 1;
            if (ts < 80 || ts > 120) return 1;
            rc = Mutex_timedlock(tt.mu, 120); // waiting
            if (rc != 0) return 1;
            ts = Hal_getTimeInMs() - ts0;
            if (ts < 180) return 1;
            Mutex_destroy(tt.mu);
            return 0;
        } break;
        case 8: { // signal
            int revents;
            Signal s = Signal_create();
            // no event
            ts0 = Hal_getTimeInMs();
            rc = Hal_pollSingle(Signal_getDescriptor(s), HAL_POLLIN, &revents, 100);
            ts = Hal_getTimeInMs() - ts0;
            if (rc != 0) return 1;
            if (ts < 80 || ts > 120) return 1;
            if (Signal_event(s) != false) return 1;
            // event
            Signal_raise(s);
            ts0 = Hal_getTimeInMs();
            rc = Hal_pollSingle(Signal_getDescriptor(s), HAL_POLLIN, &revents, -1);
            ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) return 1;
            if (ts > 20) return 1;
            if ( (revents&HAL_POLLIN) == 0) return 1;
            // test event end
            ts0 = Hal_getTimeInMs();
            rc = Hal_pollSingle(Signal_getDescriptor(s), HAL_POLLIN, &revents, -1);
            ts = Hal_getTimeInMs() - ts0;
            if (rc <= 0) return 1;
            if (ts > 20) return 1;
            if ( (revents&HAL_POLLIN) == 0) return 1;
            if (Signal_event(s) == false) return 1;
            Signal_end(s);
            // no event
            ts0 = Hal_getTimeInMs();
            rc = Hal_pollSingle(Signal_getDescriptor(s), HAL_POLLIN, &revents, 100);
            ts = Hal_getTimeInMs() - ts0;
            if (rc != 0) return 1;
            if (ts < 80 || ts > 120) return 1;
            if (Signal_event(s) != false) return 1;
            Signal_destroy(s);
            return 0;
        } break;
    }

    return 1;
}