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

int fork_launch(char* cmd);
char *read_file(const char *file, int *size);
wchar_t *get_realpath(const char *path);

#ifdef __cplusplus
}
#endif
