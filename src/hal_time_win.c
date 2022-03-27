
#if defined(_WIN32) || defined(_WIN64)

#include "hal_time.h"
#include <time.h>
#include <windows.h>

uint64_t Hal_getTimeInMs(void)
{
	FILETIME ft;
	uint64_t now;
	static const uint64_t DIFF_TO_UNIXTIME = 11644473600000LL;
	GetSystemTimeAsFileTime(&ft);
	now = (LONGLONG)ft.dwLowDateTime + ((LONGLONG)(ft.dwHighDateTime) << 32LL);
	return ((now != 0)? ((now / 10000LL) - DIFF_TO_UNIXTIME) : 0);
}

#endif // _WIN32 || _WIN64