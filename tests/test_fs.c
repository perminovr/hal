
#include <stdio.h>
#include "hal_filesystem.h"

#define err() printf("%s:%d\n", __FILE__, __LINE__)

int main(int argc, const char **argv)
{
	int test = 0;
	test = atoi(argv[1]);
    char buf[65535];
    int rc;
    FileHandle h;
    switch (test) {
        case 1: { // is not exist
            FileSystem_deleteFile("testfile");
            h = FileSystem_openFile("testfile", false);
            if (h == NULL) return 0;
        } break;
        case 2: { // rw
            // clean before
            FileSystem_deleteFile("testfile");
            // write
            h = FileSystem_openFile("testfile", true);
            if (h == NULL) { err(); return 1; }
            for (int i = 0; i < 65535; ++i) {
                buf[i] = (char)i;
            }
            rc = FileSystem_writeFile(h, buf, 65535);
            if (rc != 65535) { err(); return 1; }
            FileSystem_closeFile(h);
            // read
            memset(buf, 0, 65535);
            h = FileSystem_openFile("testfile", false);
            if (h == NULL) { err(); return 1; }
            rc = FileSystem_readFile(h, buf, 65535);
            if (rc != 65535) { err(); return 1; }
            for (int i = 0; i < 65535; ++i) {
                if (buf[i] != (char)i) { err(); return 1; }
            }
            // read with offset 257
            rc = FileSystem_readFileOffs(h, 257, buf, 256);
            for (int i = 0; i < 256; ++i) {
                if (buf[i] != ((char)(i+1))) { err(); return 1; }
            }
            FileSystem_closeFile(h);
            // clean
            rc = (int)FileSystem_deleteFile("testfile");
            if (rc == 0) { err(); return 1; }
            return 0;
        } break;
        case 3: { // rename and size
            uint32_t fileSize;
            // clean before
            FileSystem_deleteFile("testfile");
            // create
            h = FileSystem_openFile("testfile", true);
            if (h == NULL) { err(); return 1; }
            rc = FileSystem_writeFile(h, buf, 100);
            if (rc != 100) { err(); return 1; }
            FileSystem_closeFile(h);
            // rename
            rc = (int)FileSystem_renameFile("testfile", "testfile0");
            if (rc == 0) { err(); return 1; }
            // check size
            rc = (int)FileSystem_getFileInfo("testfile0", &fileSize, NULL);
            if (rc == 0) { err(); return 1; }
            if (fileSize != 100) { err(); return 1; }
            // clean
            FileSystem_deleteFile("testfile0");
            return 0;
        } break;
        case 4: { // dir
            // create dir
            //  |-testdir
            //  |---123
            //  |  |---file1
            //  |---456
            //  |---file0
            bool fnd123 = false, fdnf1 = false, fnd456 = false, fndf0 = false;
            rc = (int)FileSystem_createDirectory("testdir/123");
            if (rc == 0) { err(); return 1; }
            rc = (int)FileSystem_createDirectory("testdir/456");
            if (rc == 0) { err(); return 1; }
            h = FileSystem_openFile("testdir/123/file1", true);
            if (h == NULL) { err(); return 1; }
            FileSystem_closeFile(h);
            h = FileSystem_openFile("testdir/file0", true);
            if (h == NULL) { err(); return 1; }
            FileSystem_closeFile(h);
            // read dir
            DirectoryHandle dh = FileSystem_openDirectory("testdir");
            const char *f;
            bool isdir;
            while ( (f = FileSystem_readDirectory(dh, &isdir)) ) {
                if ( strcmp(f, "123") == 0 && isdir ) {
                    DirectoryHandle dh0 = FileSystem_openDirectory("testdir/123");
                    const char *f0;
                    bool isdir0;
                    while ( (f0 = FileSystem_readDirectory(dh0, &isdir0)) ) {
                        if ( strcmp(f0, "file1") == 0 && isdir0 == false ) {
                            fdnf1 = true;
                        }
                    }
	                FileSystem_closeDirectory(dh0);
                    fnd123 = true;
                }
                if ( strcmp(f, "456") == 0 && isdir ) {
                    fnd456 = true;
                }
                if ( strcmp(f, "file0") == 0 && isdir == false ) {
                    fndf0 = true;
                }
            }
            FileSystem_closeDirectory(dh);
            // remove dir
            rc = (int)FileSystem_deleteDirectory("testdir");
            if (rc == 0) { err(); return 1; }
            if (!fnd123 || !fdnf1 || !fnd456 || !fndf0) { err(); return 1; }
            return 0;
        } break;
        case 5: { // move
            // create dir
            rc = (int)FileSystem_createDirectory("testdir");
            if (rc == 0) { err(); return 1; }
            // move dir
            rc = (int)FileSystem_moveDirectory("testdir", "testdir0");
            if (rc == 0) { err(); return 1; }
            // check dir
            DirectoryHandle dh = FileSystem_openDirectory("testdir0");
            if (dh == NULL) { err(); return 1; }
            FileSystem_closeDirectory(dh);
            // remove dir
            rc = (int)FileSystem_deleteDirectory("testdir0");
            if (rc == 0) { err(); return 1; }
            return 0;
        } break;
    }

    { err(); return 1; }
}