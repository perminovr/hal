
#ifdef __linux__

#ifndef __USE_MISC
#define __USE_MISC
#endif
#include <endian.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define letoh32 le32toh

#define MIN(a,b) ((a)<(b)?(a):(b))

#define MD5_DIGEST_SIZE 16

typedef struct {
	union {
		uint32_t h[4];
		uint8_t digest[16];
	};
	union {
		uint32_t x[16];
		uint8_t buffer[64];
	};
	size_t size;
	uint64_t totalSize;
} Md5HashContext;

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

#include "hal_filesystem.h"


FileHandle FileSystem_openFile(const char *fileName, bool readWrite)
{
	if (fileName == NULL) return NULL;
	return (FileHandle) ((readWrite)? fopen(fileName, "ab") : fopen(fileName, "rb"));
}

FileHandle FileSystem_createFileIn(const char *path, char *filename, char *fullpath)
{
	char fn[16];
	if (!path) return NULL;
	strcpy(fn, "tempXXXXXX");
	int fd = mkstemp(fn);
	if (fd >= 0) {
		char buf[512];
		close(fd);
		sprintf(buf, "%s/%s", path, fn);
		if (filename) { strcpy(filename, fn); }
		if (fullpath) { strcpy(fullpath, buf); }
		return FileSystem_openFile(buf, true);
	}
	return NULL;
}

int FileSystem_readFile(FileHandle handle, uint8_t *buffer, int maxSize)
{
	if (handle == NULL || buffer == NULL) return -1;
	return fread(buffer, 1, maxSize, (FILE*) handle);
}

int FileSystem_readFileOffs(FileHandle handle, long offset, uint8_t *buffer, int maxSize)
{
	if (handle == NULL || buffer == NULL) return -1;
	if (fseek((FILE*) handle, offset, SEEK_SET) != 0) return -1;
	return fread(buffer, 1, maxSize, (FILE*) handle);
}

int FileSystem_writeFile(FileHandle handle, uint8_t *buffer, int size)
{
	if (handle == NULL || buffer == NULL) return -1;
	return fwrite(buffer, 1, size, (FILE*) handle);
}

void FileSystem_closeFile(FileHandle handle)
{
	if (handle == NULL) return;
	fclose((FILE*) handle);
}

bool FileSystem_deleteFile(const char *filename)
{
	if (filename == NULL) return false;
	return (remove(filename) == 0)? true : false;
}

bool FileSystem_renameFile(const char *oldFilename, const char *newFilename)
{
	if (oldFilename == NULL || newFilename == NULL) return false;
	return (rename(oldFilename, newFilename) == 0)? true : false;
}

bool FileSystem_copyFileH(FileHandle fhs, FileHandle fhd)
{
	if (!fhs || !fhd) return false;
	int rc, rc2;
	uint8_t buf[256];
	do {
		rc = FileSystem_readFile(fhs, buf, 256);
		if (rc > 0) {
			rc2 = FileSystem_writeFile(fhd, buf, rc);
			if (rc2 != rc) { return false; }
		}
	} while (rc > 0);
	return true;
}

bool FileSystem_copyFile(const char *filename, const char *newFilename)
{
	bool ret = false;
	if (!filename || !newFilename) return false;
	FileSystem_deleteFile(newFilename);
	FileHandle fhs = FileSystem_openFile(filename, false);
	FileHandle fhd = FileSystem_openFile(filename, true);
	ret = FileSystem_copyFileH(fhs, fhd);
	FileSystem_closeFile(fhs);
	FileSystem_closeFile(fhd);
	return ret;
}


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
		x[i] = letoh32(x[i]);
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

static void md5hash_init(Md5HashContext *ctx)
{
	memset(ctx, 0, sizeof(Md5HashContext));
    ctx->h[0] = 0x67452301;
    ctx->h[1] = 0xEFCDAB89;
    ctx->h[2] = 0x98BADCFE;
    ctx->h[3] = 0x10325476;
}

static void md5hash_update(Md5HashContext *ctx, const void *data, size_t length)
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

static void md5hash_final(Md5HashContext *ctx, uint8_t *digest)
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

bool FileSystem_getFileMd5Hash(const char *filename, uint8_t *hash)
{
	bool ret = false;
	if (!filename || !hash) return false;
	FileHandle fh = FileSystem_openFile(filename, false);
	if (fh) {
		int rc;
		uint8_t buf[256];
		Md5HashContext ctx;
		md5hash_init(&ctx);
		do {
			rc = FileSystem_readFile(fh, buf, 256);
			if (rc > 0) {
				md5hash_update(&ctx, buf, rc);
			}
		} while (rc > 0);
		md5hash_final(&ctx, hash);
		ret = true;
	}
	FileSystem_closeFile(fh);
	return ret;
}

bool FileSystem_getFileInfo(const char *filename, uint32_t *fileSize, uint64_t *lastModificationTimestamp)
{
	if (filename == NULL) return false;
	struct stat fileStats;

	if (stat(filename, &fileStats) == -1)
		return false;

	if (lastModificationTimestamp != NULL)
		*lastModificationTimestamp = (uint64_t) (fileStats.st_mtime) * 1000LL;
		// does not work on older systems --> *lastModificationTimestamp = (uint64_t) (fileStats.st_ctim.tv_sec) * 1000LL;

	if (fileSize != NULL)
		*fileSize = fileStats.st_size;

	return true;
}

DirectoryHandle FileSystem_openDirectory(const char *directoryName)
{
	if (directoryName == NULL) return NULL;
	return (DirectoryHandle)opendir(directoryName);
}

const char *FileSystem_readDirectory(DirectoryHandle directory, bool *isDirectory)
{
	if (directory == NULL) return NULL;
	struct dirent *dir;
	dir = readdir((DIR *)directory);
	if (dir != NULL) {
		if (dir->d_name[0] == '.')
			return FileSystem_readDirectory(directory, isDirectory);
		else {
			if (isDirectory != NULL) {
				if (dir->d_type == DT_DIR)
					*isDirectory = true;
				else
					*isDirectory = false;
			}
			return dir->d_name;
		}
	}
	else
		return NULL;
}

void FileSystem_closeDirectory(DirectoryHandle directory)
{
	if (directory == NULL) return;
	closedir((DIR *)directory);
}

static int do_mkdir(const char *path, mode_t mode)
{
	struct stat st;
	if (stat(path, &st) != 0) { // failed to get stat
		if (mkdir(path, mode) != 0) {
			return false;
		}
	} else if (!S_ISDIR(st.st_mode)) {
		return false;
	}
	return true;
}

bool FileSystem_createDirectory(const char *directoryName)
{
	mode_t mode = 0700;
	char buf[1024];
	bool ok = true;
	char *sp, *pp = buf;
	strcpy(buf, directoryName);
	while (ok && (sp = strchr(pp, '/')) != 0) {
		if (sp != pp) {
			*sp = '\0';
			ok = do_mkdir(buf, mode);
			*sp = '/';
		}
		pp = sp + 1;
	}
	ok = do_mkdir(directoryName, mode);
	return ok;
}

bool FileSystem_deleteDirectory(const char *directoryName)
{
	DirectoryHandle dh = FileSystem_openDirectory(directoryName);
	if (dh == NULL) return false;

	bool rc = false;
	const char *f;
	bool isdir;
	char *p, *fullPath = (char *)calloc(1, 512);
	sprintf(fullPath, "%s/", directoryName);
	p = fullPath + strlen(fullPath);

	while ( (f = FileSystem_readDirectory(dh, &isdir)) ) {
		if ( strcmp(f, ".") == 0 ) continue;
		if ( strcmp(f, "..") == 0 ) continue;
		strcpy(p, f);
		rc = (isdir)?
			FileSystem_deleteDirectory(fullPath) : FileSystem_deleteFile(fullPath);
		if (rc == false) {
			goto toexit;
		}
	}

toexit:
	free(fullPath);
	FileSystem_closeDirectory(dh);
	return (rmdir(directoryName) == 0)? true : false;
}


bool FileSystem_moveDirectory(const char *dirName, const char *newDirName)
{
	return (rename(dirName, newDirName) == 0)? true : false;
}

#endif // __linux__