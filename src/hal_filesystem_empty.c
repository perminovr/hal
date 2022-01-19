#include "hal_base.h"

#ifndef HAL_NOT_EMPTY

#include "hal_filesystem.h"


FileHandle FileSystem_openFile(const char *fileName, bool readWrite)
{
	return NULL;
}

int FileSystem_readFile(FileHandle handle, uint8_t *buffer, int maxSize)
{
	return -1;
}

int FileSystem_writeFile(FileHandle handle, uint8_t *buffer, int size)
{
	return -1;
}

void FileSystem_closeFile(FileHandle handle)
{
}

bool FileSystem_deleteFile(const char *filename)
{
	return false;
}

bool FileSystem_renameFile(const char *oldFilename, char *newFilename)
{
	return false;
}


bool FileSystem_getFileInfo(const char *filename, uint32_t *fileSize, uint64_t *lastModificationTimestamp)
{
	return false;
}

DirectoryHandle FileSystem_openDirectory(const char *directoryName)
{
	return NULL;
}

char *FileSystem_readDirectory(DirectoryHandle directory, bool *isDirectory)
{
	return NULL;
}

void FileSystem_closeDirectory(DirectoryHandle directory)
{
}

#endif // HAL_NOT_EMPTY