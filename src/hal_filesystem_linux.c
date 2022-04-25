
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

#include "hal_filesystem.h"
#include "hal_utils.h"


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
		FileSystem_deleteFile(fn);
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

static bool FileSystem_copyFileH(const char *filename, FileHandle fhd)
{
	if (!filename || !fhd) return false;
	int rc, rc2;
	uint8_t buf[256];
	if (fseek((FILE*) fhd, 0, SEEK_SET) != 0) return false;
	FileHandle fhs = FileSystem_openFile(filename, false);
	do {
		rc = FileSystem_readFile(fhs, buf, 256);
		if (rc > 0) {
			rc2 = FileSystem_writeFile(fhd, buf, rc);
			if (rc2 != rc) {
				FileSystem_closeFile(fhs);
				FileSystem_closeFile(fhd);
				return false;
			}
		}
	} while (rc > 0);
	FileSystem_closeFile(fhs);
	FileSystem_closeFile(fhd);
	return true;
}

bool FileSystem_copyFile(const char *filename, const char *newFilename)
{
	if (!filename || !newFilename) return false;
	FileSystem_deleteFile(newFilename);
	FileHandle fhd = FileSystem_openFile(newFilename, true);
	return FileSystem_copyFileH(filename, fhd);
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