#include "cosuplatform.h"
#include <stdio.h>
#include <windows.h>

int fork_launch(char* cmd)
{
    return 0; // stub
}

char *read_file(const char *file, int *size)
{
    return NULL; // stub;
}

char *get_realpath(const char *path) // mostly duplicate of get_rootpath in wsigscan
{
    LPTSTR pathbuf = (LPTSTR) calloc(MAX_PATH, sizeof(TCHAR));
    if (pathbuf == NULL)
    {
        fputs("Failed allocating memory for path!\n", stderr);
        return NULL;
    }

    DWORD err = GetFullPathName(path, MAX_PATH, pathbuf, NULL);
    if (err >= MAX_PATH)
    {
        fputs("Windows may have truncated file path!\n", stderr);
    }
    else if (err == 0)
    {
        fprintf(stderr, "Failed getting process path: %lu\n", GetLastError());
        return NULL;
    }

    err++; // add a null terminator

#ifdef _UNICODE // just convert them into char
    char *apath = (char*) malloc(err);
    if (apath == NULL)
    {
        fputs("Failed allocating memory!\n", stderr);
        return NULL;
    }
    for (DWORD i = 0; i < err; i++)
    {
        if (*(pathbuf + i) > 127)
        {
            fputs("There is an unsupported character in the path!\n", stderr);
            free(apath);
            apath = NULL;
            break;
        }
        *(apath + i) = *(pathbuf + i);
    } // check if err includes null-terminator
    free(pathbuf);
    return apath;
#else
    LPTSTR real = (LPTSTR) realloc(pathbuf, sizeof(TCHAR) * err);
    if (real == NULL) return (char*) pathbuf;
    else return (char*) real;
#endif
}
