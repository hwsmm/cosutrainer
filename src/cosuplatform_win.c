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

char *get_realpath(const char *path)
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
        free(wcbuf);
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

    int mbnum = WideCharToMultiByte(CP_UTF8, 0, pathbuf, -1, NULL, 0, NULL, NULL);
    if (mbnum == 0)
    {
        fputs("Failed converting!\n", stderr);
        free(pathbuf);
        return NULL;
    }

    LPSTR mbbuf = (LPSTR) malloc(mbnum);
    if (mbbuf == NULL)
    {
        fputs("Failed allocation\n", stderr);
        free(pathbuf);
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, pathbuf, -1, mbbuf, mbnum, NULL, NULL) == 0)
    {
        fputs("Failed converting!\n", stderr);
        free(pathbuf);
        free(mbbuf);
        return NULL;
    }

    free(pathbuf);

    return mbbuf;
}

char *get_songspath()
{
    return NULL;
    // if one ever needs to implement it, it needs to be for custom songs folder
    // as osu! songs folder is normally osu! running folder\Songs, you need process handle we don't have here
}
