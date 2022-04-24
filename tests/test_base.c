
#include <stdio.h>
#include "hal_base.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
	switch (test) {
		case 1: {
			if (Hal_unidescIsInvalid( Hal_getInvalidUnidesc() )) return 0;
		} break;
		case 2: {
			unidesc ud1 = Hal_getInvalidUnidesc();
			unidesc ud2 = Hal_getInvalidUnidesc();
			if (Hal_unidescIsEqual( &ud1, &ud2 )) return 0;
		} break;
	}

	{ err(); return 1; }
}