#pragma once

#ifdef WIN32

#include <windows.h>
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
#define _access access
#define _strdup strdup

#define COSU_IPCKEY ftok("/", 727)
#define COSU_IPCREQ 1
#define COSU_IPCPATH 2

#endif

#include <wchar.h>

#ifdef __cplusplus
extern "C"
{
#endif

int execute_file(char* file);
char *read_file(const char *file, int *size);
char *get_songspath(wchar_t *base_path);
int try_convertwinpath(char *path, int pos);
char *alloc_wcstombs(wchar_t *wide);
wchar_t *alloc_mbstowcs(char *multi);
char *convert_to_cp932(char *str);

#ifdef __cplusplus
}
#endif
