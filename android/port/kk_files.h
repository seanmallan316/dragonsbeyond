// kk_files.h - included first in every game file on Android: the game's files are read straight out of the app
// (no unpacking, no second copy on the phone). Paths starting with "content/" open inside the APK.
#ifndef KK_FILES_H
#define KK_FILES_H
#include <stdio.h>
FILE *kk_fopen(const char *path, const char *mode);
void kk_files_init(void);
#define fopen kk_fopen
#endif
