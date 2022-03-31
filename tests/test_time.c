
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
    }

    { err(); return 1; }
}