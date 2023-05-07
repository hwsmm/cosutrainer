//#define OSUMEM_PREAD

#ifndef OSUMEM_PREAD
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/uio.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>

#define CMPSTR(x, y) (strncmp(x, y, sizeof((y)) - 1) == 0)
#define printerr(s) fputs(s "\n", stderr)

#include "sigscan.h"

void init_sigstatus(struct sigscan_status *st)
{
    st->status = -1;
    st->osu = -1;
#ifdef OSUMEM_PREAD
    st->mem_fd = -1;
#endif
}

void find_and_set_osu(struct sigscan_status *st)
{
    if (st->osu > 0 && kill(st->osu, 0) == 0)
    {
        st->status = 1;
        return;
    }

    pid_t osupid;
    char pidbuf[16] = {0};
    int ref = -3;

    pid_t candidate = 0;
    DIR *d;
    struct dirent *ent;
    int commfd;

    if (chdir("/proc"))
    {
        perror("/proc");
        ref = -1;
    }
    else
    {
        d = opendir(".");
        if (!d)
        {
            perror("/proc");
            ref = -1;
        }
        else
        {
            while ((ent = readdir(d)))
            {
                candidate = atoi(ent->d_name);
                if (candidate > 0)
                {
                    if (!chdir(ent->d_name))
                    {
                        commfd = open("comm", O_RDONLY);
                        if (commfd == -1)
                        {
                            printerr("Error while opening file");
                            continue;
                        }
                        ssize_t readbytes = read(commfd, &pidbuf, 16);
                        if (close(commfd) < 0)
                        {
                            printerr("Error while closing file");
                        }

                        if (readbytes != -1)
                        {
                            if (CMPSTR(pidbuf, "osu!.exe"))
                            {
                                osupid = candidate;
                                ref = 0;
                                break;
                            }
                        }
                        if (chdir(".."))
                        {
                            perror("..");
                            ref = -1;
                            break;
                        }
                    }
                }
            }
            if (closedir(d))
            {
                perror("/proc");
            }
        }
    }

    if (chdir("/tmp"))
    {
        perror("/tmp");
        ref = -1;
    }

    if (ref < 0)
    {
        st->status = ref;
        return;
    }

    if (osupid != st->osu)
    {
        st->osu = osupid;
        st->status = 2;
        return;
    }
    st->status = 1;
}

bool is_osu_alive(struct sigscan_status *st)
{
    return st->status >= 0 && st->osu > 0 && kill(st->osu, 0) == 0;
}

#ifndef OSUMEM_PREAD
int init_memread(struct sigscan_status *st)
{
    return 1;
}

bool stop_memread(struct sigscan_status *st)
{
    return true;
}

bool readmemory(struct sigscan_status *st, ptr_type address, void *buffer, size_t len)
{
    struct iovec local;
    local.iov_base = buffer;
    local.iov_len = len;

    struct iovec remote;
    remote.iov_base = (void*) address;
    remote.iov_len = len;

    if (process_vm_readv(st->osu, &local, 1, &remote, 1, 0) == -1)
    {
        perror(NULL);
        return false;
    }

    return true;
}
#else
int init_memread(struct sigscan_status *st)
{
    char procmem[32];
    int fd;
    snprintf(procmem, sizeof procmem, "/proc/%d/mem", st->osu);
    fd = open(procmem, O_RDONLY);

    if (fd == -1)
    {
        perror(procmem);
        st->status = -1;
        st->osu = -1;
        return -3;
    }

    st->mem_fd = fd;
    return fd;
}

bool stop_memread(struct sigscan_status *st)
{
    if (st->mem_fd != -1 && close(st->mem_fd) == -1)
    {
        perror("proc_mem");
        return false;
    }
    st->mem_fd = -1;
    return true;
}

bool readmemory(struct sigscan_status *st, ptr_type address, void *buffer, size_t len)
{
    if (pread(st->mem_fd, buffer, len, (off_t) address) == -1)
    {
        perror(NULL);
        return false;
    }
    return true;
}
#endif

ptr_type find_pattern(struct sigscan_status *st, const uint8_t bytearray[], const unsigned int pattern_size, const bool mask[])
{
    char mapsfile[32];
    FILE *maps;
    char line[1024];
    ptr_type result = NULL;

    snprintf(mapsfile, sizeof mapsfile, "/proc/%d/maps", st->osu);

    maps = fopen(mapsfile, "r");
    if (!maps)
    {
        perror(mapsfile);
        return NULL;
    }

    while (fgets(line, sizeof line, maps))
    {
        if (result != NULL) break;

        char *startstr = strtok(line, "-");
        char *endstr = strtok(NULL, " ");
        char *permstr = strtok(NULL, " ");

        if (!startstr || !endstr || !permstr) continue;

        if (*permstr != 'r')
        {
            continue;
        }

        ptr_type startptr = (ptr_type) strtol(startstr, NULL, 16);
        ptr_type endptr = (ptr_type) strtol(endstr, NULL, 16);

        unsigned long size = (unsigned long) endptr - (unsigned long) startptr + 1L;

        if (size < pattern_size)
        {
            continue;
        }

        uint8_t *buffer = (uint8_t*) malloc(size);
        if (buffer == NULL)
        {
            printerr("Failed allocating memory");
            break;
        }

        if (!readmemory(st, startptr, buffer, size))
        {
            free(buffer);
            continue;
        }

        unsigned long i;
        for (i = 0; i < size - pattern_size + 1L; i++)
        {
            unsigned int e;
            bool match = true;
            for (e = 0; e < pattern_size; e++)
            {
                if (*(buffer + i + e) != bytearray[e] && (mask == NULL || mask[e] != true))
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                result = ptr_add(startptr, i);
                break;
            }
        }
        free(buffer);
    }

    if (fclose(maps) == EOF)
    {
        perror(mapsfile);
    }

    return result;
}

wchar_t *get_rootpath(struct sigscan_status *st)
{
    return NULL;
}
