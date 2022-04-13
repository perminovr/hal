
#if defined(_WIN32) || defined(_WIN64)

#include "hal_time.h"
#include <time.h>
#include <windows.h>

static const int64_t DIFF_TO_UNIXTIME = 11644473600000LL;

int Hal_getRealTimeSpec(struct timespechal *ts)
{
	uint64_t now;
	GetSystemTimeAsFileTime((FILETIME*)&now);
	now -= DIFF_TO_UNIXTIME;
	ts->tv_sec = (uint32_t)(now / 10000000LL);
	ts->tv_nsec = (uint32_t)((now % 10000000LL) * 100);
	return 0;
}

uint64_t Hal_getTimeInMs(void)
{
	uint64_t now;
	GetSystemTimeAsFileTime((FILETIME*)&now);
	return ((now != 0)? ((now / 10000LL) - DIFF_TO_UNIXTIME) : 0);
}

uint64_t Hal_getMonotonicTimeInMs(void)
{
	LARGE_INTEGER Time;
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency); 
	QueryPerformanceCounter(&Time);
	return (uint64_t)((Time.QuadPart * 1000) / Frequency.QuadPart);
}

#endif // _WIN32 || _WIN64