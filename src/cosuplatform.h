#pragma once

#ifdef WIN32
#include <io.h>
#define PATHSEP '\\'
#define STR_PATHSEP "\\"
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#define PATHSEP '/'
#define STR_PATHSEP "/"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

int fork_launch(char* cmd);
char *read_file(const char *file, int *size);
char *get_realpath(const char *path);

#ifdef __cplusplus
}
#endif
