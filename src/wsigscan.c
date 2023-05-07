#include "sigscan.h"
#include <windows.h>
#include <memoryapi.h>
#include <string.h>
#include <wtsapi32.h>
#include <psapi.h>
#include <tchar.h>

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

    WTS_PROCESS_INFO* pWPIs = NULL;
    DWORD dwProcCount = 0;
    DWORD osupid = 0;
    if (WTSEnumerateProcesses(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pWPIs, &dwProcCount))
    {
        for (DWORD i = 0; i < dwProcCount; i++)
        {
            if (_tcscmp((pWPIs + i)->pProcessName, _T("osu!.exe")) == 0)
            {
                osupid = (pWPIs + i)->ProcessId;
                break;
            }
        }
    }

    if (pWPIs)
    {
        WTSFreeMemory(pWPIs);
    }

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

bool readmemory(struct sigscan_status *st, ptr_type address, void *buffer, unsigned long len)
{
    if (!ReadProcessMemory(st->osuproc, (LPCVOID) address, buffer, len, NULL))
    {
        fprintf(stderr, "Failed reading memory: %lu\n", GetLastError());
        return false;
    }
    return true;
}

ptr_type find_pattern(struct sigscan_status *st, const uint8_t bytearray[], unsigned int pattern_size, const bool mask[])
{
    MEMORY_BASIC_INFORMATION info;
    LPVOID curaddr = NULL;
    LPVOID result = NULL;
    uint8_t *buffer = NULL;
    SIZE_T cursize = 0;

    // check first if we can get memory map from the process
    if (VirtualQueryEx(st->osuproc, curaddr, &info, sizeof(info)) == 0)
    {
        fprintf(stderr, "Failed getting virtual memory map: %lu\n", GetLastError());
        return PTR_NULL;
    }
    buffer = (uint8_t*) malloc(info.RegionSize);
    if (buffer == NULL)
    {
        fputs("Failed allocating memory while initializing buffer while looking for a pattern\n", stderr);
        return PTR_NULL;
    }
    cursize = info.RegionSize;

    do
    {
        if (result != NULL) break;

        curaddr = ptr_add(info.BaseAddress, info.RegionSize);
        if ((info.State & 0x1000) == 0 || (info.Protect & 0x100) != 0) continue;

        if (info.RegionSize < pattern_size) continue;
        if (info.RegionSize > cursize)
        {
            // didn't use realloc since i don't need to retain data from old buffer
            free(buffer);
            buffer = (uint8_t*) malloc(info.RegionSize);
            if (buffer == NULL)
            {
                fputs("Failed allocating a new buffer\n", stderr);
                return PTR_NULL;
            }
            cursize = info.RegionSize;
        }
        if (ReadProcessMemory(st->osuproc, info.BaseAddress, buffer, info.RegionSize, NULL))
        {
            for (SIZE_T i = 0; i < info.RegionSize - pattern_size; i++)
            {
                bool match = true;
                for (SIZE_T e = 0; e < pattern_size; e++)
                {
                    if (*(buffer + i + e) != bytearray[e] && (mask == NULL || mask[e] != true))
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                {
                    result = ptr_add(info.BaseAddress, i);
                    break;
                }
            }
        }
    }
    while (VirtualQueryEx(st->osuproc, curaddr, &info, sizeof(info)) == sizeof(info));
    free(buffer);

    return (ptr_type) result;
}

wchar_t *get_rootpath(struct sigscan_status *st)
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

    LPWSTR real = (LPWSTR) realloc(pathbuf, sizeof(WCHAR) * (wcslen(pathbuf) + 1));
    if (real == NULL) return pathbuf;
    else return real;
}
