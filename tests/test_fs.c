
#include <stdio.h>
#include "hal_filesystem.h"
#include "hal_utils.h"

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
		case 6: { // create file in
			char filename[256];
			char fullpath[256];
			rc = (int)FileSystem_createDirectory("testdir");
			if (rc == 0) { err(); return 1; }
			h = FileSystem_createFileIn("testdir", filename, fullpath);
			if (h == NULL) { err(); return 1; }
			if (strstr(fullpath, "testdir") == NULL) { err(); return 1; }
			if (strlen(filename) == 0) { err(); return 1; }
			FileSystem_closeFile(h);
			rc = (int)FileSystem_deleteFile(fullpath);
			if (rc == 0) { err(); return 1; }
			rc = (int)FileSystem_deleteDirectory("testdir");
			if (rc == 0) { err(); return 1; }
			return 0;
		} break;
		case 7: { // file copy
			// clean before
			FileSystem_deleteFile("testfile");
			FileSystem_deleteFile("testfile0");
			// write
			h = FileSystem_openFile("testfile", true);
			if (h == NULL) { err(); return 1; }
			for (int i = 0; i < 65535; ++i) {
				buf[i] = (char)i;
			}
			rc = FileSystem_writeFile(h, buf, 65535);
			if (rc != 65535) { err(); return 1; }
			FileSystem_closeFile(h);
			// copy
			rc = (int)FileSystem_copyFile("testfile", "testfile0");
			if (rc == 0) { err(); return 1; }
			// read
			memset(buf, 0, 65535);
			h = FileSystem_openFile("testfile0", false);
			if (h == NULL) { err(); return 1; }
			rc = FileSystem_readFile(h, buf, 65535);
			if (rc != 65535) { err(); return 1; }
			for (int i = 0; i < 65535; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			FileSystem_closeFile(h);
			// clean
			rc = (int)FileSystem_deleteFile("testfile");
			if (rc == 0) { err(); return 1; }
			rc = (int)FileSystem_deleteFile("testfile0");
			if (rc == 0) { err(); return 1; }
			return 0;
		} break;
		case 8: { // file copy h
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
			// close
			FileSystem_closeFile(h);
			// copy
			char fullpath[256];
			FileHandle h2 = FileSystem_createFileIn(".", NULL, fullpath);
			if (h2 == NULL) { err(); return 1; }
			rc = (int)FileSystem_copyFileH("testfile", h2);
			if (rc == 0) { err(); return 1; }
			// reopen
			h2 = FileSystem_openFile(fullpath, false);
			// read
			memset(buf, 0, 65535);
			rc = FileSystem_readFile(h2, buf, 65535); printf("%d\n", rc);
			if (rc != 65535) { err(); return 1; }
			for (int i = 0; i < 65535; ++i) {
				if (buf[i] != (char)i) { err(); return 1; }
			}
			// clean
			FileSystem_closeFile(h2);
			rc = (int)FileSystem_deleteFile("testfile");
			if (rc == 0) { err(); return 1; }
			rc = (int)FileSystem_deleteFile(fullpath);
			if (rc == 0) { err(); return 1; }
			return 0;
		} break;
		case 9: { // file md5
			const char *text = "ljalsml;masldjdh1u876tr19oghqbv szc9j9\naskc9j0asjciasc'aasp=d=12-e12e254894619+*";
			const char *realsum = "2e537c05b009d2d64db2eb597191aa51";
			const char *p = realsum;
			char buf[3];
			uint8_t realhash[16];
			uint8_t hash[16];
			// clean before
			FileSystem_deleteFile("testfile");
			// write
			h = FileSystem_openFile("testfile", true);
			if (h == NULL) { err(); return 1; }
			rc = FileSystem_writeFile(h, (uint8_t *)text, (int)strlen(text));
			if (rc != strlen(text)) { err(); return 1; }
			// close
			FileSystem_closeFile(h);
			// compute hash
			rc = (int)FileSystem_getFileMd5Hash("testfile", hash);
			if (rc == 0) { err(); return 1; }
			// check
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