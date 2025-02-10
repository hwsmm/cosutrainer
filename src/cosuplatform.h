#pragma once
#include <wchar.h>

#ifdef WIN32
#include <io.h>
#include <synchapi.h>
#define PATHSEP '\\'
#define STR_PATHSEP "\\"
#define F_OK 0
#define ssleep(x) Sleep(x)
#else
#include <unistd.h>
#define PATHSEP '/'
#define STR_PATHSEP "/"
#define ssleep(x) usleep(x * 1000)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

int execute_file(char* file);
char *read_file(const char *file, int *size);
char *get_realpath(char *path);
char *get_songspath();
char *get_iconpath();
int try_convertwinpath(char *path, int pos);
char *alloc_wcstombs(wchar_t *wide);
wchar_t *alloc_mbstowcs(char *multi);

#ifdef __cplusplus
}
#endif
