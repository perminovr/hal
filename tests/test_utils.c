
#include <stdio.h>
#include "hal_utils.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
	switch (test) {
		case 1: { // prefix
			if (NetwHlpr_maskToPrefix("255.255.0.0") != 16) { err(); return 1; }
			if (NetwHlpr_maskToPrefix("255.255.255.252") != 30) { err(); return 1; }
			if (NetwHlpr_maskToPrefix("128.0.0.0") != 1) { err(); return 1; }
			return 0;
		} break;
		case 2: { // mask
			char buf[16];
			if (strcmp(NetwHlpr_prefixToMask(16, buf), "255.255.0.0") != 0) { err(); return 1; }
			if (strcmp(NetwHlpr_prefixToMask(30, buf), "255.255.255.252") != 0) { err(); return 1; }
			if (strcmp(NetwHlpr_prefixToMask(1, buf), "128.0.0.0") != 0) { err(); return 1; }
			return 0;
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
			uint16_t port = NetwHlpr_generatePort("abcdef", 20000, 22000);
			if (port >= 20000 && port <= 22000) return 0;
		} break;
		case 7: {
			uint16_t port = NetwHlpr_generatePort("abcdef", 10000, 12000);
			if (port == 11624) return 0;
		} break;
		case 8: { // brdcst
			if (NetwHlpr_broadcast("192.168.1.1", 16) != 0xffffa8c0) { err(); return 1; }
			if (NetwHlpr_broadcast("192.168.1.1", 8) != 0xffffffc0) { err(); return 1; }
			if (NetwHlpr_broadcast("192.168.100.1", 20) != 0xff6fa8c0) { err(); return 1; }
			return 0;
		} break;
		case 9: { // subnet
			if (NetwHlpr_subnet("192.168.1.1", 16) != 0xa8c0) { err(); return 1; }
			if (NetwHlpr_subnet("192.168.1.1", 8) != 0xc0) { err(); return 1; }
			if (NetwHlpr_subnet("192.168.100.1", 20) != 0x60a8c0) { err(); return 1; }
			return 0;
		} break;
		case 10: { // md5
			const char *text = "ljalsml;masldjdh1u876tr19oghqbv szc9j9\naskc9j0asjciasc'aasp=d=12-e12e254894619+*";
			const char *realsum = "2e537c05b009d2d64db2eb597191aa51";
			const char *p = realsum;
			char buf[3];
			uint8_t realhash[16];
			uint8_t hash[16];
			Md5HashContext ctx;
			md5hash_init(&ctx);
			md5hash_update(&ctx, text, strlen(text));
			md5hash_final(&ctx, hash);
			for (int i = 0; i < 16; ++i) {
				buf[0] = p[0];
				buf[1] = p[1];
				buf[2] = '\0';
				realhash[i] = (uint8_t)strtoul(buf, NULL, 16);
				if (realhash[i] != hash[i]) { err(); return 1; }
				p+=2;
			}			
			return 0;
		} break;
	}

	{ err(); return 1; }
}