
#include <stdio.h>
#include "hal_time.h"
#include "hal_thread.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
    switch (test) {
        case 1: {
            if (Hal_getTimeInMs() != 0) return 0;
        } break;
        case 2: {
            uint64_t ts, ts0;
            ts0 = Hal_getTimeInMs();
			Thread_sleep(100);
            ts = Hal_getTimeInMs() - ts0;
            if (ts < 80 || ts > 120) { err(); return 1; }
			return 0;
        } break;
        case 3: {
			struct timespechal tspec0;
			struct timespechal tspec;
			while (1) { // simplifying the test
				Hal_getRealTimeSpec(&tspec0);
				if (tspec0.tv_nsec > 200000000U && tspec0.tv_nsec < 800000000U)
					break;
				Thread_sleep(1);
			}
			Thread_sleep(1000);
			Hal_getRealTimeSpec(&tspec);
			uint32_t ndiff = (tspec.tv_nsec > tspec0.tv_nsec)?
					tspec.tv_nsec - tspec0.tv_nsec : tspec0.tv_nsec - tspec.tv_nsec;
            if ((tspec.tv_sec - tspec0.tv_sec) != 1) { err(); return 1; }
            if (ndiff > 20000000U) { err(); return 1; }
			return 0;
        } break;
    }

    { err(); return 1; }
}