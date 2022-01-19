#include "hal_base.h"

#ifndef HAL_NOT_EMPTY

bool Hal_unidescIsInvalid(unidesc p)
{
	return false;
}

unidesc Hal_getInvalidUnidesc(void)
{
	unidesc p;
	p.i32 = 0;
	return p;
}

bool Hal_unidescIsEqual(const unidesc *p1, const unidesc *p2)
{
	return false;
}

#endif // HAL_NOT_EMPTY