#pragma once
#ifdef WIN32
#include <windows.h>
#else
#include <limits.h>
#include <sys/types.h>
#include <stdbool.h>
#endif

#include <stdint.h>

#ifdef WIN32
typedef LPVOID ptr_type;
#else
typedef void* ptr_type;
#endif
#define PTR_SIZE 4 // sizeof(ptr_type) is not used since osu is a 32bit game, but requires 64bit pointers to function normally for some reason?
#define PTR_NULL NULL
#define ptr_add(x, offset) ((void*) ((long long) x + offset)) // to fix pointer_arith warnings

#ifdef __cplusplus
extern "C"
{
#endif

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

void init_sigstatus(struct sigscan_status *st);
int init_memread(struct sigscan_status *st);
bool stop_memread(struct sigscan_status *st);

#define OSUMEM_NOT_FOUND(x) ((x)->status < 0)
#define OSUMEM_OK(x) ((x)->status >= 0)
#define OSUMEM_NEW_OSU(x) ((x)->status == 2)
#define OSUMEM_RUNNING(x) ((x)->status == 1)

void find_and_set_osu(struct sigscan_status *st);
bool is_osu_alive(struct sigscan_status *st);
bool readmemory(struct sigscan_status *st, ptr_type address, void *buffer, unsigned long len);
ptr_type find_pattern(struct sigscan_status *st, const uint8_t bytearray[], unsigned int pattern_size, const bool mask[]);
char *get_rootpath(struct sigscan_status *st);

#ifdef __cplusplus
}
#endif
