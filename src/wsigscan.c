#include "sigscan.h"
#include <windows.h>
#include <memoryapi.h>
#include <string.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>

void init_sigstatus(struct sigscan_status *st)
{
    st->status = -1;
    st->osu = 0;
    st->osuproc = NULL;
}

bool is_osu_alive(struct sigscan_status *st)
{
    if (st->osu > 0 && st->osuproc != NULL)
    {
        DWORD ret;
        GetExitCodeProcess(st->osuproc, &ret);
        return ret == STILL_ACTIVE;
    }
    return false;
}

void find_and_set_osu(struct sigscan_status *st)
{
    if (is_osu_alive(st))
    {
        st->status = 1;
        return;
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snap == INVALID_HANDLE_VALUE)
    {
        fputs("Failed getting process list!\n", stderr);
        st->status = -1;
        return;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    bool first = true;
    DWORD osupid = 0;
    while (first ? Process32FirstW(snap, &pe) && !(first = false) : Process32NextW(snap, &pe))
    {
        if (lstrcmpiW(pe.szExeFile, L"osu!.exe") == 0)
        {
            osupid = pe.th32ProcessID;
            break;
        }
    }

    CloseHandle(snap);

    if (osupid > 0)
    {
        if (st->osu == osupid)
        {
            st->status = 1;
            return;
        }

        st->osu = osupid;
        st->status = 2;
        return;
    }
    st->status = -1;
}

int init_memread(struct sigscan_status *st)
{
    if (st->osu <= 0) return -1;

    st->osuproc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, st->osu);
    if (st->osuproc == NULL)
    {
        fprintf(stderr, "Failed opening a process: %lu\n", GetLastError());
        st->status = -1;
        st->osu = 0;
        return -2;
    }
    return 1;
}

bool stop_memread(struct sigscan_status *st)
{
    return CloseHandle(st->osuproc);
}

bool readmemory(struct sigscan_status *st, ptr_type address, void *buffer, size_t len)
{
    if (!ReadProcessMemory(st->osuproc, (LPCVOID) address, buffer, len, NULL))
    {
        fprintf(stderr, "Failed reading memory: %lu\n", GetLastError());
        return false;
    }
    return true;
}

struct winmemit
{
    struct sigscan_status *st;
    LPVOID curaddr;
};

void *start_regionit(struct sigscan_status *st)
{
    struct winmemit *it = (struct winmemit*) malloc(sizeof(struct winmemit));
    if (it == NULL)
    {
        return NULL;
    }

    it->st = st;
    it->curaddr = NULL;
    return it;
}

int next_regionit(void *regionit, struct vm_region *res)
{
    struct winmemit *it = (struct winmemit*) regionit;
    MEMORY_BASIC_INFORMATION info;

    while (1)
    {
        if (VirtualQueryEx(it->st->osuproc, it->curaddr, &info, sizeof(info)) != sizeof(info))
        {
            return 0;
        }

        it->curaddr = ptr_add(info.BaseAddress, info.RegionSize);

        if ((info.State & 0x1000) == 0 || (info.Protect & 0x100) != 0)
        {
            continue;
        }

        res->start = info.BaseAddress;
        res->len = info.RegionSize;
        return 1;
    }
}

void stop_regionit(void *regionit)
{
    free(regionit);
}

#ifndef KEYOVERLAY
char *get_rootpath(struct sigscan_status *st)
{
    LPWSTR pathbuf = (LPWSTR) calloc(MAX_PATH, sizeof(WCHAR));
    if (pathbuf == NULL)
    {
        fputs("Failed allocating memory for process path!\n", stderr);
        return NULL;
    }

    DWORD err = GetModuleFileNameExW(st->osuproc, NULL, pathbuf, MAX_PATH);
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
            *findslash = '\0';
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

    return mbbuf;
}
#endif
