
#ifdef __linux__

#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hal_filesystem.h"


FileHandle FileSystem_openFile(const char *fileName, bool readWrite)
{
	if (fileName == NULL) return NULL;
	return (FileHandle) ((readWrite)? fopen(fileName, "ab") : fopen(fileName, "rb"));
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
	mode_t mode = 0007;
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