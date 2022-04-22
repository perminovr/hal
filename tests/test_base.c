
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
		case 3: {
			if (Hal_ipv4StrToBin("0.0.0.0") == 0) return 0;
		} break;
		case 4: {
			if (Hal_ipv4StrToBin("255.255.255.255") == 0xffffffff) return 0;
		} break;
		case 5: {
			if (Hal_ipv4StrToBin("127.0.0.1") == 0x100007f) return 0;
		} break;
		case 6: {
			uint16_t port = Hal_generatePort("abcdef", 20000, 22000);
			if (port >= 20000 && port <= 22000) return 0;
		} break;
		case 7: {
			uint16_t port = Hal_generatePort("abcdef", 10000, 12000);
			if (port == 11624) return 0;
		} break;
	}

	{ err(); return 1; }
}