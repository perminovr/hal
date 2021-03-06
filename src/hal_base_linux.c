#ifdef __linux__

#include "hal_base.h"

bool Hal_unidescIsInvalid(unidesc p)
{
	return (p.i32 == -1)? true : false;
}

unidesc Hal_getInvalidUnidesc(void)
{
	unidesc p;
	p.i32 = -1;
	return p;
}

bool Hal_unidescIsEqual(const unidesc *p1, const unidesc *p2)
{
	return (p1 && p2) && (p1->i32 == p2->i32)? true : false;
}

#endif // __linux__