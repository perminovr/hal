
#ifdef __linux__
# ifndef __USE_MISC
#  define __USE_MISC
# endif
# include <endian.h>
# include <arpa/inet.h>
#endif

#if defined (_WIN32) || defined(_WIN64)
# include <winsock2.h>
# include <ws2tcpip.h>
# if BYTE_ORDER == LITTLE_ENDIAN
#  if defined(_MSC_VER)
#   define htobe16(x) _byteswap_ushort(x)
#   define htole16(x) (x)
#   define be16toh(x) _byteswap_ushort(x)
#   define le16toh(x) (x)
#   define htobe32(x) _byteswap_ulong(x)
#   define htole32(x) (x)
#   define be32toh(x) _byteswap_ulong(x)
#   define le32toh(x) (x)
#   define htobe64(x) _byteswap_uint64(x)
#   define htole64(x) (x)
#   define be64toh(x) _byteswap_uint64(x)
#   define le64toh(x) (x)
#  elif defined(__GNUC__) || defined(__clang__)
#   define htobe16(x) __builtin_bswap16(x)
#   define htole16(x) (x)
#   define be16toh(x) __builtin_bswap16(x)
#   define le16toh(x) (x)
#   define htobe32(x) __builtin_bswap32(x)
#   define htole32(x) (x)
#   define be32toh(x) __builtin_bswap32(x)
#   define le32toh(x) (x)
#   define htobe64(x) __builtin_bswap64(x)
#   define htole64(x) (x)
#   define be64toh(x) __builtin_bswap64(x)
#   define le64toh(x) (x)
#  endif
# endif
#endif
#include <stdio.h>

#include "hal_utils.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MD5_DIGEST_SIZE 16

//MD5 auxiliary functions
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))

#define ROL32(A, n)	(((A) << (n)) | (((A)>>(32-(n)))  & ((1UL << (n)) - 1)))
#define ROR32(A, n)	ROL32((A), 32-(n))

#define FF(a, b, c, d, x, s, k) a += F(b, c, d) + (x) + (k), a = ROL32(a, s) + (b)
#define GG(a, b, c, d, x, s, k) a += G(b, c, d) + (x) + (k), a = ROL32(a, s) + (b)
#define HH(a, b, c, d, x, s, k) a += H(b, c, d) + (x) + (k), a = ROL32(a, s) + (b)
#define II(a, b, c, d, x, s, k) a += I(b, c, d) + (x) + (k), a = ROL32(a, s) + (b)

//MD5 padding
static const uint8_t padding[64] =
{
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//MD5 constants
static const uint32_t k[64] =
{
	0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
	0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
	0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
	0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
	0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
	0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
	0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
	0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391
};

static void processBlock(Md5HashContext *ctx)
{
	unsigned int i;

	//Initialize the 4 working registers
	uint32_t a = ctx->h[0];
	uint32_t b = ctx->h[1];
	uint32_t c = ctx->h[2];
	uint32_t d = ctx->h[3];

	//Process message in 16-word blocks
	uint32_t *x = ctx->x;

	//Convert from little-endian byte order to host byte order
	for(i = 0; i < 16; i++) {
		x[i] = le32toh(x[i]);
	}

	//Round 1
	FF(a, b, c, d, x[0],  7,  k[0]);
	FF(d, a, b, c, x[1],  12, k[1]);
	FF(c, d, a, b, x[2],  17, k[2]);
	FF(b, c, d, a, x[3],  22, k[3]);
	FF(a, b, c, d, x[4],  7,  k[4]);
	FF(d, a, b, c, x[5],  12, k[5]);
	FF(c, d, a, b, x[6],  17, k[6]);
	FF(b, c, d, a, x[7],  22, k[7]);
	FF(a, b, c, d, x[8],  7,  k[8]);
	FF(d, a, b, c, x[9],  12, k[9]);
	FF(c, d, a, b, x[10], 17, k[10]);
	FF(b, c, d, a, x[11], 22, k[11]);
	FF(a, b, c, d, x[12], 7,  k[12]);
	FF(d, a, b, c, x[13], 12, k[13]);
	FF(c, d, a, b, x[14], 17, k[14]);
	FF(b, c, d, a, x[15], 22, k[15]);

	//Round 2
	GG(a, b, c, d, x[1],  5,  k[16]);
	GG(d, a, b, c, x[6],  9,  k[17]);
	GG(c, d, a, b, x[11], 14, k[18]);
	GG(b, c, d, a, x[0],  20, k[19]);
	GG(a, b, c, d, x[5],  5,  k[20]);
	GG(d, a, b, c, x[10], 9,  k[21]);
	GG(c, d, a, b, x[15], 14, k[22]);
	GG(b, c, d, a, x[4],  20, k[23]);
	GG(a, b, c, d, x[9],  5,  k[24]);
	GG(d, a, b, c, x[14], 9,  k[25]);
	GG(c, d, a, b, x[3],  14, k[26]);
	GG(b, c, d, a, x[8],  20, k[27]);
	GG(a, b, c, d, x[13], 5,  k[28]);
	GG(d, a, b, c, x[2],  9,  k[29]);
	GG(c, d, a, b, x[7],  14, k[30]);
	GG(b, c, d, a, x[12], 20, k[31]);

	//Round 3
	HH(a, b, c, d, x[5],  4,  k[32]);
	HH(d, a, b, c, x[8],  11, k[33]);
	HH(c, d, a, b, x[11], 16, k[34]);
	HH(b, c, d, a, x[14], 23, k[35]);
	HH(a, b, c, d, x[1],  4,  k[36]);
	HH(d, a, b, c, x[4],  11, k[37]);
	HH(c, d, a, b, x[7],  16, k[38]);
	HH(b, c, d, a, x[10], 23, k[39]);
	HH(a, b, c, d, x[13], 4,  k[40]);
	HH(d, a, b, c, x[0],  11, k[41]);
	HH(c, d, a, b, x[3],  16, k[42]);
	HH(b, c, d, a, x[6],  23, k[43]);
	HH(a, b, c, d, x[9],  4,  k[44]);
	HH(d, a, b, c, x[12], 11, k[45]);
	HH(c, d, a, b, x[15], 16, k[46]);
	HH(b, c, d, a, x[2],  23, k[47]);

	//Round 4
	II(a, b, c, d, x[0],  6,  k[48]);
	II(d, a, b, c, x[7],  10, k[49]);
	II(c, d, a, b, x[14], 15, k[50]);
	II(b, c, d, a, x[5],  21, k[51]);
	II(a, b, c, d, x[12], 6,  k[52]);
	II(d, a, b, c, x[3],  10, k[53]);
	II(c, d, a, b, x[10], 15, k[54]);
	II(b, c, d, a, x[1],  21, k[55]);
	II(a, b, c, d, x[8],  6,  k[56]);
	II(d, a, b, c, x[15], 10, k[57]);
	II(c, d, a, b, x[6],  15, k[58]);
	II(b, c, d, a, x[13], 21, k[59]);
	II(a, b, c, d, x[4],  6,  k[60]);
	II(d, a, b, c, x[11], 10, k[61]);
	II(c, d, a, b, x[2],  15, k[62]);
	II(b, c, d, a, x[9],  21, k[63]);

	//Update the hash value
	ctx->h[0] += a;
	ctx->h[1] += b;
	ctx->h[2] += c;
	ctx->h[3] += d;
}

void md5hash_init(Md5HashContext *ctx)
{
	memset(ctx, 0, sizeof(Md5HashContext));
	ctx->h[0] = 0x67452301;
	ctx->h[1] = 0xEFCDAB89;
	ctx->h[2] = 0x98BADCFE;
	ctx->h[3] = 0x10325476;
}

void md5hash_update(Md5HashContext *ctx, const void *data, size_t length)
{
	size_t n;
	//Process the incoming data
	while(length > 0) {
		//The buffer can hold at most 64 bytes
		n = MIN(length, 64 - ctx->size);

		//Copy the data to the buffer
		memcpy(ctx->buffer + ctx->size, data, n);

		//Update the MD5 ctx
		ctx->size += n;
		ctx->totalSize += n;
		//Advance the data pointer
		data = (uint8_t *) data + n;
		//Remaining bytes to process
		length -= n;

		//Process message in 16-word blocks
		if(ctx->size == 64) {
			//Transform the 16-word block
			processBlock(ctx);
			//Empty the buffer
			ctx->size = 0;
		}
	}
}

void md5hash_final(Md5HashContext *ctx, uint8_t *digest)
{
	unsigned int i;
	size_t paddingSize;
	uint64_t totalSize;

	//Length of the original message (before padding)
	totalSize = ctx->totalSize * 8;

	//Pad the message so that its length is congruent to 56 modulo 64
	if(ctx->size < 56) {
		paddingSize = 56 - ctx->size;
	} else {
		paddingSize = 64 + 56 - ctx->size;
	}

	//Append padding
	md5hash_update(ctx, padding, paddingSize);

	//Append the length of the original message
	ctx->x[14] = htole32((uint32_t) totalSize);
	ctx->x[15] = htole32((uint32_t) (totalSize >> 32));

	//Calculate the message digest
	processBlock(ctx);

	//Convert from host byte order to little-endian byte order
	for(i = 0; i < 4; i++) {
		ctx->h[i] = htole32(ctx->h[i]);
	}

	//Copy the resulting digest
	if(digest != NULL) {
		memcpy(digest, ctx->digest, MD5_DIGEST_SIZE);
	}
}


uint8_t NetwHlpr_maskToPrefix(const char *strMask)
{
	uint8_t ret;
	uint32_t prefix = 0;
	uint32_t mask = 0;
	if (strMask) {
		if (strlen(strMask) <= 2) {
			prefix = 32;
			prefix = (uint32_t)strtoul(strMask, NULL, 10);
			mask = (0xFFFFFFFF << (32 - prefix)) & 0xFFFFFFFF;
			mask = htonl(mask);
		} else {
			mask = ntohl(inet_addr(strMask));
			for (int i = 31; i >= 0; --i) {
				if (mask & (1 << i)) {
					prefix++;
				} else {
					break;
				}
			}
		}
	}
	ret = (uint8_t)prefix;
	return ret;
}

char *NetwHlpr_prefixToMask(int prefix, char *buf)
{
	if (prefix >= 0 && prefix <= 32) {
		uint32_t mask = prefix ? (0xFFFFFFFF << (32 - prefix)) & 0xFFFFFFFF : 0;
		mask = htonl(mask);
		return inet_ntop(AF_INET, &mask, buf, 16)? buf : NULL;
	}
	return NULL;
}

uint32_t NetwHlpr_broadcast(const char *ip, uint8_t prefix)
{
	uint32_t ret = 0;
	if (ip) {
		ret = htonl(inet_addr(ip));
		if (ret) {
			for (int i = 0; i < 32-prefix; ++i) {
				ret |= (1 << i);
			}
		}
		ret = htonl(ret);
	}
	return ret;
}

uint32_t NetwHlpr_subnet(const char *ip, uint8_t prefix)
{
	uint32_t mask = (0xFFFFFFFF << (32-prefix));
	return htonl(htonl(inet_addr(ip)) & mask);
}

uint16_t NetwHlpr_generatePort(const char *name, uint16_t min, uint16_t max)
{
	const uint16_t table[256] = {
		0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
		0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
		0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
		0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
		0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
		0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
		0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
		0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
		0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
		0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
		0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
		0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
		0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
		0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
		0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
		0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
		0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
		0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
		0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
		0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
		0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
		0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
		0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
		0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
		0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
		0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
		0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
		0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
		0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
		0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
		0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
		0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
	};
	uint16_t ret = 0xFFFF;
	uint16_t cnt = 0;
	if (min > max) {
		uint16_t tmp = min;
		min = max;
		max = tmp;
	}
	uint16_t diff = max - min;
	if (diff < 999) return 0;
	int len = (int)strlen(name);
	while (ret < min || ret > max) {
		uint8_t vxor = 0;
		const char *p = name;
		int l = len;
		ret += cnt;
		while (l--) {
			vxor = (*p++) ^ ret;
			ret >>= 8;
			ret ^= table[vxor];
		}
		cnt++;
		if (cnt > diff) return 0;
	}
	return ret;
}


uint32_t Hal_ipv4StrToBin(const char *ip)
{
	return (uint32_t)inet_addr(ip);
}

char *Hal_ipv4BinToStr(uint32_t in, char *out)
{
	union { uint32_t u32; uint8_t b[4]; } addr;
	addr.u32 = in;
	if ( sprintf(out, "%hhu.%hhu.%hhu.%hhu",
			addr.b[3], addr.b[2], addr.b[1], addr.b[0]) == 4 )
	{
		return out;
	}
	return NULL;
}
