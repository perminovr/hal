
#if defined(_WIN32) || defined(_WIN64)

#include "hal_time.h"
#include <time.h>
#include <windows.h>

static const int64_t DIFF_TO_UNIXTIME = 11644473600000LL;

int Hal_getRealTimeSpec(struct timespechal *ts)
{
	int64_t now;
	GetSystemTimeAsFileTime((FILETIME*)&now);
	now -= DIFF_TO_UNIXTIME;
	ts->tv_sec = now / 10000000LL;
	ts->tv_nsec = (now % 10000000LL) * 100;
	return 0;
}

uint64_t Hal_getTimeInMs(void)
{
	FILETIME ft;
	uint64_t now;
	GetSystemTimeAsFileTime(&ft);
	now = (LONGLONG)ft.dwLowDateTime + ((LONGLONG)(ft.dwHighDateTime) << 32LL);
	return ((now != 0)? ((now / 10000LL) - DIFF_TO_UNIXTIME) : 0);
}

#endif // _WIN32 || _WIN64