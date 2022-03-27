
#if defined(_WIN32) || defined(_WIN64)

#include "hal_base.h"

bool Hal_unidescIsInvalid(unidesc p)
{
	return (p.u64 == 0ULL)? true : false;
}

unidesc Hal_getInvalidUnidesc(void)
{
	unidesc p;
	p.u64 = 0ULL;
	return p;
}

bool Hal_unidescIsEqual(const unidesc *p1, const unidesc *p2)
{
	return (p1 && p2) && (p1->u64 == p2->u64)? true : false;
}

#include <ws2tcpip.h>

uint32_t Hal_ipv4StrToBin(const char *ip)
{
	uint32_t ret = 0;
	inet_pton(AF_INET, ip, &ret);
	return ret;
}

#endif // _WIN32 || _WIN64