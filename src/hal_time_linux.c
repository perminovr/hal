
#ifdef __linux__

#include "hal_time.h"
#include <time.h>
#include <sys/time.h>

uint64_t Hal_getTimeInMs(void)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return ((uint64_t) now.tv_sec * 1000LL) + (now.tv_usec / 1000);
}

#endif // __linux__