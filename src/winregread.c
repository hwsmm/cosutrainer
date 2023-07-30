#include "winregread.h"
#include <stdio.h>
#include <shlwapi.h>
#include <lmcons.h>
#include <locale.h>
#include <strsafe.h>
#include <wchar.h>

LPWSTR getOsuPath(LPDWORD len)
{
    DWORD size = 0;
    LSTATUS ret = RegGetValueW(HKEY_CLASSES_ROOT, L"osu\\shell\\open\\command", L"", RRF_RT_REG_SZ, NULL, NULL, &size);
    if (ret != ERROR_SUCCESS)
    {
        fprintf(stderr, "Failed reading registry: %d\n", ret);
        return NULL;
    }

    LPWSTR path = (LPWSTR) calloc(size, sizeof(WCHAR));
    if (path == NULL)
    {
        fprintf(stderr, "Failed allocation\n");
        return NULL;
    }

    ret = RegGetValueW(HKEY_CLASSES_ROOT, L"osu\\shell\\open\\command", L"", RRF_RT_REG_SZ, NULL, path, &size);
    if (ret != ERROR_SUCCESS)
    {
        fprintf(stderr, "Failed reading registry: %d\n", ret);
        free(path);
        return NULL;
    }

    // https://github.com/ppy/osu/blob/master/osu.Desktop/OsuGameDesktop.cs#L87
    PWSTR find = StrRChrW(path, NULL, L' ');
    if (find != NULL) *find = '\0';

    find = StrRChrW(path, NULL, L'"');
    if (find != NULL) *find = '\0';

    PWSTR start = StrChrW(path, L'"');
    if (start == NULL) start = path;
    else start++;

    find = StrStrIW(path, L"osu!.exe");
    if (find == NULL)
    {
        fprintf(stderr, "Registry value is not proper!");
        free(path);
        return NULL;
    }
    *find = '\0';

    DWORD i;
    for (i = 0; i < size; i++)
    {
        *(path + i) = *(start + i);
        if (*(start + i) == '\0')
        {
            break;
        }
    }

    LPWSTR tmp = (LPWSTR) realloc(path, i * sizeof(WCHAR));
    if (tmp == NULL)
    {
        fprintf(stderr, "Failed reallocation\n");
        free(path);
        return NULL;
    }

    *len = i;

    return tmp;
}

LPWSTR getOsuSongsPath(LPWSTR osupath, DWORD pathsize)
{
    WCHAR uname[UNLEN + 1];
    DWORD size = UNLEN + 1;
    if (GetUserNameW(uname, &size) == 0)
    {
        fprintf(stderr, "Failed getting user name: %d", GetLastError());
        return NULL;
    }

    size = pathsize + 1 + (sizeof("osu!.") - 1) + (size - 1) + (sizeof(".cfg") - 1) + 1;
    wchar_t *cfgpath = (wchar_t*) calloc(size, sizeof(wchar_t));
    if (cfgpath == NULL)
    {
        fprintf(stderr, "Failed allocation!\n");
        return NULL;
    }

    StringCchPrintfW(cfgpath, size, L"%ls\\osu!.%ls.cfg", osupath, uname);

    FILE *fp = _wfopen(cfgpath, L"r");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed opening %ls\n", cfgpath);
        free(cfgpath);
        return NULL;
    }
    free(cfgpath);

    wchar_t line[4096];
    wchar_t find[] = L"BeatmapDirectory = ";
    wchar_t *path = NULL;
    while (fgetws(line, 4096, fp) != NULL)
    {
        if (wcsncmp(line, find, sizeof(find)/sizeof(find[0]) - 1) == 0)
        {
            wchar_t *f = wcsrchr(line, L'\n');
            if (f != NULL && *(f - 1) == '\r') *(f - 1) = '\0';
            else if (f != NULL) *f = '\0';

            path = line + sizeof(find)/sizeof(find[0]) - 1;
            break;
        }
    }

    fclose(fp);

    if (path == NULL)
    {
        fprintf(stderr, "BeatmapDirectory not found in config!\n");
        return NULL;
    }

    size_t psize = 0;
    if (StringCchLengthW(path, STRSAFE_MAX_CCH, &psize) != S_OK)
    {
        fprintf(stderr, "Error calculating string length\n");
        return NULL;
    }

    LPWSTR res = NULL;
    DWORD ressize = 0;
    if (PathIsRelativeW(path))
    {
        ressize = (pathsize - 1) + 1 + psize + 1;
        res = (LPWSTR) calloc(ressize, sizeof(WCHAR));
        if (res == NULL)
        {
            fprintf(stderr, "Failed allocation!\n");
            return NULL;
        }
        StringCchCopyW(res, pathsize, osupath);
        StringCchCatW(res, pathsize + 1, L"\\");
        StringCchCatW(res, pathsize + psize + 1, path);
    }
    else
    {
        ressize = psize + 1;
        res = (LPWSTR) calloc(ressize, sizeof(WCHAR));
        if (res == NULL)
        {
            fprintf(stderr, "Failed allocation!\n");
            return NULL;
        }
        StringCchCopyW(res, ressize, path);
    }
    return res;
}

#ifdef WINE
int main(int argc, char *argv[])
{
    setlocale(LC_CTYPE, ".UTF-8");

    DWORD sz = 0;
    LPWSTR st = getOsuPath(&sz);
    if (st == NULL) return 1;
    LPWSTR rs = getOsuSongsPath(st, sz);
    if (rs == NULL)
    {
        free(st);
        return 2;
    }

    printf("%ls", rs);
    free(st);
    free(rs);
    return 0;
}
#endif
