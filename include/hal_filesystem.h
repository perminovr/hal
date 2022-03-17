#ifndef HAL_FILESYSTEM_H
#define HAL_FILESYSTEM_H

#include "hal_base.h"

#ifdef __cplusplus
extern "C" {
#endif


/*! \addtogroup hal
   *
   *  @{
   */

/**
 * @defgroup HAL_FILESYSTEM Interface to the native file system (optional)
 *
 * @{
 */


typedef void *FileHandle;
typedef void *DirectoryHandle;


/**
 * \brief open a file
 *
 * \param pathName full name (path + filename) of the file
 * \param readWrite true opens the file with read and write access - false opens for read access only
 *
 * \return a handle for the file. Has to be used by subsequent calls to file functions to identify the file or
 *		 NULL if opening fails
 */
HAL_API FileHandle
FileSystem_openFile(const char *pathName, bool readWrite);

/**
 * \brief read from an open file
 *
 * This function will read the next block of the file. The maximum number of bytes to read
 * is given. A call to this function will move the file position by the number of bytes read.
 * If the file position reaches the end of file then subsequent calls of this function shall
 * return 0.
 *
 * \param handle the file handle to identify the file
 * \param buffer the buffer to write the read data
 * \param maxSize maximum number of bytes to read
 *
 * \return the number of bytes actually read
 */
HAL_API int
FileSystem_readFile(FileHandle handle, uint8_t *buffer, int maxSize);

/**
 * \brief read from an open file by given offset
 *
 * If the file position reaches the end of file then subsequent calls of this function shall
 * return 0.
 *
 * \param handle the file handle to identify the file
 * \param offset offset within the file from the start
 * \param buffer the buffer to write the read data
 * \param maxSize maximum number of bytes to read
 *
 * \return the number of bytes actually read
 */
HAL_API int
FileSystem_readFileOffs(FileHandle handle, long offset, uint8_t *buffer, int maxSize);

/**
 * \brief write to an open file
 *
 * \param handle the file handle to identify the file
 * \param buffer the buffer with the data to write
 * \param size the number of bytes to write
 *
 * \return the number of bytes actually written
 */
HAL_API int
FileSystem_writeFile(FileHandle handle, uint8_t *buffer, int size);

/**
 * \brief close an open file
 *
 * \param handle the file handle to identify the file
 */
HAL_API void
FileSystem_closeFile(FileHandle handle);

/**
 * \brief return attributes of the given file
 *
 * This function is used by the MMS layer to determine basic file attributes.
 * The size of the file has to be returned in bytes. The timestamp of the last modification has
 * to be returned as milliseconds since Unix epoch - or 0 if this function is not supported.
 *
 * \param pathName full name (path + filename) of the file
 * \param fileSize a pointer where to store the file size
 * \param lastModificationTimestamp is used to store the timestamp of last modification of the file
 *
 * \return true if file exists, false if not
 */
HAL_API bool
FileSystem_getFileInfo(const char *filename, uint32_t *fileSize, uint64_t *lastModificationTimestamp);

/**
 * \brief delete a file
 *
 * \param pathName full name (path + filename) of the file
 *
 * \return true on success, false on error
 */
HAL_API bool
FileSystem_deleteFile(const char *filename);

/**
 * \brief rename a file
 *
 * \param oldFileName current full name (path + filename) of the file
 * \param newFileName new full name (path + filename) of the file
 *
 * \return true on success, false on error
 */
HAL_API bool
FileSystem_renameFile(const char *oldFilename, const char *newFilename);

/**
 * \brief open the directoy with the specified name
 *
 * \param directoryName
 *
 * \return a handle for the opened directory to be used in subsequent calls to identify the directory
 */
HAL_API DirectoryHandle
FileSystem_openDirectory(const char *directoryName);

/**
 * \brief read the next directory entry
 *
 * This function returns the next directory entry. The entry is only a valid pointer as long as the
 * FileSystem_closeDirectory or another FileSystem_readDirectory function is not called for the given
 * DirectoryHandle.
 *
 * \param directory the handle to identify the directory
 * \param isDirectory return value that indicates if the directory entry is itself a directory (true)
 *
 * \return the name of the directory entry
 */
HAL_API const char *
FileSystem_readDirectory(DirectoryHandle directory, bool *isDirectory);


/**
 * \brief close a directory
 *
 * \param directory the handle to identify the directory
 */
HAL_API void
FileSystem_closeDirectory(DirectoryHandle directory);

/**
 * \brief create the directoy with the specified name
 *
 * \param directoryName
 *
 * \return true on success, false on error
 */
HAL_API bool
FileSystem_createDirectory(const char *directoryName);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif

#endif /* HAL_FILESYSTEM_H */
