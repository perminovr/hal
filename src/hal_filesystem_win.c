
#if defined(_WIN32) || defined(_WIN64)

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>

#include "hal_filesystem.h"

struct DirectoryHandle_s {
    HANDLE h;
    WIN32_FIND_DATA ffd;
    bool fl;
};


FileHandle FileSystem_openFile(const char *fileName, bool readWrite)
{
	if (fileName == NULL) return NULL;
	return (FileHandle) ((readWrite)? fopen(fileName, "ab") : fopen(fileName, "rb"));
}

int FileSystem_readFile(FileHandle handle, uint8_t *buffer, int maxSize)
{
	if (handle == NULL || buffer == NULL) return -1;
	return (int)fread(buffer, 1, maxSize, (FILE*) handle);
}

int FileSystem_readFileOffs(FileHandle handle, long offset, uint8_t *buffer, int maxSize)
{
	if (handle == NULL || buffer == NULL) return -1;
	if (fseek((FILE*) handle, offset, SEEK_SET) != 0) return -1;
	return (int)fread(buffer, 1, maxSize, (FILE*) handle);
}

int FileSystem_writeFile(FileHandle handle, uint8_t *buffer, int size)
{
	if (handle == NULL || buffer == NULL) return -1;
	return (int)fwrite(buffer, 1, size, (FILE*) handle);
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
	WIN32_FIND_DATA ffd;
    char fullPath[MAX_PATH + 1];
	struct DirectoryHandle_s *h = NULL;
	HANDLE dir;
	if (directoryName == NULL) return NULL;
	if (strlen(directoryName) >= MAX_PATH) return NULL;
	strncpy(fullPath, directoryName, MAX_PATH-3);
	strncat(fullPath, "\\*", MAX_PATH);
	dir = FindFirstFile(fullPath, &ffd);
	if (dir != INVALID_HANDLE_VALUE) {
		h = (struct DirectoryHandle_s *)calloc(1, sizeof(struct DirectoryHandle_s));
		h->h = dir;
		memcpy(&h->ffd, &ffd, sizeof(WIN32_FIND_DATA));
		h->fl = true;
	}
	return (DirectoryHandle)h;
}

const char *FileSystem_readDirectory(DirectoryHandle directory, bool *isDirectory)
{
	struct DirectoryHandle_s *h = (struct DirectoryHandle_s *)directory;
	if (directory == NULL) return NULL;
	if (h->fl == false) {
		if ( FindNextFile(h->h, &h->ffd) == false ) return NULL;
	}
	h->fl = false;
	if (isDirectory) *isDirectory = (h->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)?
				true : false;
	return (const char *)h->ffd.cFileName;
}

void FileSystem_closeDirectory(DirectoryHandle directory)
{
	if (directory == NULL) return;
	struct DirectoryHandle_s *h = (struct DirectoryHandle_s *)directory;
	FindClose(h->h);
	free(h);
}

// from GNU C Library
#ifndef __S_IFDIR
#define	__S_IFDIR	0040000	/* Directory.  */
#endif
#ifndef __S_IFMT
#define	__S_IFMT	0170000	/* These bits determine file type.  */
#endif
#ifndef __S_ISTYPE
#define	__S_ISTYPE(mode, mask)	(((mode) & __S_IFMT) == (mask))
#endif
#ifndef S_ISDIR
#define	S_ISDIR(mode)	 __S_ISTYPE((mode), __S_IFDIR)
#endif

static int do_mkdir(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0) { // failed to get stat
		BOOL rc = CreateDirectoryA(path, NULL);
		DWORD e = GetLastError();
		if (rc == 0 && e != ERROR_ALREADY_EXISTS) {
			return false;
		}
	} else if (!S_ISDIR(st.st_mode)) {
		return false;
	}
	return true;
}

bool FileSystem_createDirectory(const char *directoryName)
{
	char buf[1024];
	bool ok = true;
	char *sp, *pp = buf;
	strcpy(buf, directoryName);
	while (ok && (sp = strchr(pp, '/')) != 0) {
		if (sp != pp) {
			*sp = '\0';
			ok = do_mkdir(buf);
			*sp = '/';
		}
		pp = sp + 1;
	}
	ok = do_mkdir(directoryName);
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
	sprintf(fullPath, "%s\\", directoryName);
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
	return RemoveDirectoryA(directoryName);
}


bool FileSystem_moveDirectory(const char *dirName, const char *newDirName)
{
	char dir0[512];
	char dir1[512];
	strcpy(dir0, dirName);
	strcpy(dir1, newDirName);
	for (int i = 0; i < 512; ++i) {
		if (dir0[i] == '/') dir0[i] = '\\';
		if (dir1[i] == '/') dir1[i] = '\\';
	}
	return MoveFileA(dir0, dir1);
}

#endif // _WIN32 || _WIN64