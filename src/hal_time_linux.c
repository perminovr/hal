
#ifdef __linux__

#include "hal_time.h"
#include <time.h>
#include <sys/time.h>

uint64_t Hal_getTimeInMs(void)
{
	struct timeval now;
	int rc = gettimeofday(&now, NULL);
	return ((rc == 0)? (((uint64_t) now.tv_sec * 1000LL) + (now.tv_usec / 1000)) : 0);
}

#endif // __linux__