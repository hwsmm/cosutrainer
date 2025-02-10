#pragma once
#ifdef WIN32
#include <windows.h>
#else
#include <limits.h>
#include <sys/types.h>
#endif

#include <inttypes.h>
#include <wchar.h>
#include <stdio.h>
#include <stdbool.h>

typedef void* ptr_type;
#define PTR_SIZE 4 // sizeof(ptr_type) is not used since osu is a 32bit game, but requires 64bit pointers to function normally for some reason?
#define PTR_NULL NULL
#define ptr_add(x, offset) ((ptr_type) ((intptr_t) x + offset)) // to fix pointer_arith warnings

struct sigscan_status
{
    int status;

#ifdef WIN32
    DWORD osu;
    HANDLE osuproc;
#else
    pid_t osu;
#ifdef OSUMEM_PREAD
    int mem_fd;
#endif
#endif
};

struct vm_region
{
    ptr_type start;
    size_t len;
};

#ifdef __cplusplus
extern "C"
{
#endif

void init_sigstatus(struct sigscan_status *st);
int init_memread(struct sigscan_status *st);
bool stop_memread(struct sigscan_status *st);

#define OSUMEM_NOT_FOUND(x) ((x)->status < 0)
#define OSUMEM_OK(x) ((x)->status >= 0)
#define OSUMEM_NEW_OSU(x) ((x)->status == 2)
#define OSUMEM_RUNNING(x) ((x)->status == 1)

#define DEFAULT_LOGIC(st, NEW_FOUND, SUCCESS, LOST_OSU) \
if (OSUMEM_NOT_FOUND(st)) \
{ \
    find_and_set_osu(st); \
    if (OSUMEM_OK(st)) \
    { \
        if (init_memread(st) == -1) \
        { \
            fputs("Failed initializing memory reader\n", stderr); \
        } \
        else \
        { \
            NEW_FOUND; \
        } \
    } \
} \
else \
{ \
    find_and_set_osu(st); \
    if (OSUMEM_NOT_FOUND(st)) \
    { \
        stop_memread(st); \
        LOST_OSU; \
    } \
} \
if (OSUMEM_OK(st)) \
{ \
    SUCCESS; \
}

void find_and_set_osu(struct sigscan_status *st);
bool is_osu_alive(struct sigscan_status *st);
bool readmemory(struct sigscan_status *st, ptr_type address, void *buffer, size_t len);
char *get_rootpath(struct sigscan_status *st);

void *start_regionit(struct sigscan_status *st);
int next_regionit(void *regionit, struct vm_region *res);
void stop_regionit(void *regionit);

ptr_type find_pattern(struct sigscan_status *st, const uint8_t bytearray[], const unsigned int pattern_size, const bool mask[]);

#ifdef __cplusplus
}
#endif
