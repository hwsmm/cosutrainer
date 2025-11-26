#include "cosuplatform.h"
#include "winregread.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/platform.h>
#include <stdio.h>
#include <windows.h>
#include <strsafe.h>

int execute_file(char* file)
{
    HWND handle = NULL;
    const Fl_Window *fwin = Fl_Window::current();
    if (fwin != NULL)
        handle = fl_xid(fwin);

    LPWSTR wfile = alloc_mbstowcs(file);
    ShellExecuteW(handle, L"open", wfile, NULL, NULL, 0);
    free(wfile);
    return 0;
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

    LPWSTR wcbuf = alloc_mbstowcs(path);
    if (wcbuf == NULL)
    {
        fputs("Failed allocating memory for path!\n", stderr);
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

    LPSTR mbbuf = alloc_wcstombs(pathbuf);
    free(pathbuf);
    return mbbuf;
}

char *get_songspath(wchar_t *base_path)
{
    LPWSTR st = base_path;
    if (st == NULL)
    {
        fputs("Failed to get osu! directory from process, falling back to defaults...\n", stderr);
        st = getOsuPath();
    }
    if (st == NULL) return NULL;
    LPWSTR rs = getOsuSongsPath(st);
    if (rs == NULL)
    {
        free(st);
        return NULL;
    }
    free(st);

    LPSTR mbbuf = alloc_wcstombs(rs);
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

    LPSTR mbbuf = alloc_wcstombs(pathbuf);
    free(pathbuf);
    return mbbuf;
}

static LPSTR alloc_wcstombs_internal(LPWSTR wide, UINT codepage)
{
    int mbnum = WideCharToMultiByte(codepage, 0, wide, -1, NULL, 0, NULL, NULL);
    if (mbnum == 0)
    {
        fputs("Failed converting!\n", stderr);
        return NULL;
    }

    LPSTR mbbuf = (LPSTR) malloc(mbnum);
    if (mbbuf == NULL)
    {
        fputs("Failed allocation\n", stderr);
        return NULL;
    }

    if (WideCharToMultiByte(codepage, 0, wide, -1, mbbuf, mbnum, NULL, NULL) == 0)
    {
        fputs("Failed converting!\n", stderr);
        free(mbbuf);
        return NULL;
    }

    return mbbuf;
}

LPSTR alloc_wcstombs(LPWSTR wide)
{
    return alloc_wcstombs_internal(wide, CP_UTF8);
}

LPWSTR alloc_mbstowcs(LPSTR multi)
{
    int wcnum = MultiByteToWideChar(CP_UTF8, 0, multi, -1, NULL, 0);
    if (wcnum == 0)
    {
        fputs("Failed converting!\n", stderr);
        return NULL;
    }

    LPWSTR wcbuf = (LPWSTR) calloc(wcnum, sizeof(WCHAR));
    if (wcbuf == NULL)
    {
        fputs("Failed allocating memory for path!\n", stderr);
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, multi, -1, wcbuf, wcnum) == 0)
    {
        fputs("Failed converting!\n", stderr);
        free(wcbuf);
        return NULL;
    }

    return wcbuf;
}

char *convert_to_cp932(char *str)
{
    LPWSTR wstr = alloc_mbstowcs(str);
    if (wstr != NULL)
    {
        LPSTR cv = alloc_wcstombs_internal(wstr, 932);
        free(wstr);
        return cv;
    }

    return NULL;
}
