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

wchar_t *get_realpath(const char *path)
{
    LPWSTR pathbuf = (LPWSTR) calloc(MAX_PATH, sizeof(WCHAR));
    if (pathbuf == NULL)
    {
        fputs("Failed allocating memory for path!\n", stderr);
        return NULL;
    }

    int wcnum = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wcnum == 0)
    {
        fputs("Failed converting!\n", stderr);
        free(pathbuf);
        return NULL;
    }
    LPWSTR wcbuf = (LPWSTR) calloc(wcnum, sizeof(WCHAR));
    if (wcbuf == NULL)
    {
        fputs("Failed allocating memory for path!\n", stderr);
        free(pathbuf);
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wcbuf, wcnum) == 0)
    {
        fputs("Failed converting!\n", stderr);
        free(pathbuf);
        return NULL;
    }

    DWORD err = GetFullPathNameW(wcbuf, MAX_PATH, pathbuf, NULL);
    if (err >= MAX_PATH)
    {
        fputs("Windows may have truncated file path!\n", stderr);
    }
    else if (err == 0)
    {
        fprintf(stderr, "Failed getting process path: %lu\n", GetLastError());
        free(wcbuf);
        free(pathbuf);
        return NULL;
    }
    free(wcbuf);

    *(pathbuf + err) = '\0';

    LPWSTR real = (LPWSTR) realloc(pathbuf, sizeof(WCHAR) * (err + 1));
    if (real == NULL) return pathbuf;
    else return real;
}
