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
    if (_access(path, F_OK) == 0)
    {
        return 1;
    }
    return -1;
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
