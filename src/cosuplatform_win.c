#include "cosuplatform.h"
#include "winregread.h"
#include <stdio.h>
#include <windows.h>
#include <strsafe.h>

int fork_launch(char* cmd)
{
    return 0; // stub
}

char *read_file(const char *file, int *size)
{
    return NULL; // stub;
}

char *get_realpath(char *path)
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
    DWORD sz = 0;
    LPWSTR st = getOsuPath(&sz);
    if (st == NULL) return NULL;
    LPWSTR rs = getOsuSongsPath(st, sz);
    if (rs == NULL)
    {
        free(st);
        return NULL;
    }
    free(st);

    int mbnum = WideCharToMultiByte(CP_UTF8, 0, rs, -1, NULL, 0, NULL, NULL);
    if (mbnum == 0)
    {
        fprintf(stderr, "Failed converting (%d)!\n", GetLastError());
        free(rs);
        return NULL;
    }

    LPSTR mbbuf = (LPSTR) malloc(mbnum);
    if (mbbuf == NULL)
    {
        fputs("Failed allocation\n", stderr);
        free(rs);
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, rs, -1, mbbuf, mbnum, NULL, NULL) == 0)
    {
        fprintf(stderr, "Failed converting (%d)!\n", GetLastError());
        free(rs);
        free(mbbuf);
        return NULL;
    }
    free(rs);
    return mbbuf;
}

// stub, since winapi functions already handle them well enough
int try_convertwinpath(char *path, int pos)
{
    if (access(path, F_OK) == 0)
    {
        return 1;
    }
    return -1;
}

char *get_iconpath()
{
    const WCHAR name[] = L"cosutrainer.png";

    LPWSTR pathbuf = (LPWSTR) calloc(MAX_PATH, sizeof(WCHAR));
    if (pathbuf == NULL)
    {
        fputs("Failed allocating memory for process path!\n", stderr);
        return NULL;
    }

    DWORD err = GetModuleFileNameW(NULL, pathbuf, MAX_PATH);
    if (err >= MAX_PATH)
    {
        fputs("Windows may have truncated file path!\n", stderr);
    }
    else if (err == 0)
    {
        fprintf(stderr, "Failed getting process path: %lu\n", GetLastError());
        return NULL;
    }

    LPWSTR findslash = pathbuf + wcslen(pathbuf);
    while (*--findslash)
    {
        if (*findslash == '\\')
        {
            StringCchCopyW(findslash + 1, MAX_PATH - err - 1, name);
            break;
        }
        err--;
    }

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
