#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include "cosumem.h"
#include "tools.h"

#ifdef OSUMEM_PREAD
struct sigscan_status st = { -1, -1, -1 };
#else
struct sigscan_status st = { -1, -1 };
#endif

void* match_pattern()
{
    // "F8 01 74 04 83 65"
    const uint8_t basepattern[] = { 0xf8, 0x01, 0x74, 0x04, 0x83, 0x65 };
    return find_pattern(&st, basepattern, 6, NULL);
}

ptr_type get_beatmap_ptr(ptr_type base_address)
{
    ptr_type beatmap_2ptr = PTR_NULL;
    ptr_type beatmap_ptr = PTR_NULL;

    if (!readmemory(&st, ptr_add(base_address, -0xC), &beatmap_2ptr, PTR_SIZE))
        return PTR_NULL;

    if (!readmemory(&st, beatmap_2ptr, &beatmap_ptr, PTR_SIZE))
        return PTR_NULL;

    return beatmap_ptr;
}

int get_mapid(ptr_type base_address)
{
    int id = 0;
    if (!readmemory(&st, ptr_add(get_beatmap_ptr(base_address), 0xCC), &id, 4))
        return -1;

    return id;
}

char *get_mappath(ptr_type base_address, unsigned int *length)
{
    ptr_type beatmap_ptr = get_beatmap_ptr(base_address);
    if (beatmap_ptr == PTR_NULL) return PTR_NULL;

    ptr_type folder_ptr = PTR_NULL;
    ptr_type path_ptr = PTR_NULL;
    int foldersize = 0;
    int pathsize = 0;

    if (!readmemory(&st, ptr_add(beatmap_ptr, 0x78), &folder_ptr, PTR_SIZE))
        return NULL;

    if (!readmemory(&st, ptr_add(folder_ptr, 4), &foldersize, 4))
        return NULL;

    if (!readmemory(&st, ptr_add(beatmap_ptr, 0x94), &path_ptr, PTR_SIZE))
        return NULL;

    if (!readmemory(&st, ptr_add(path_ptr, 4), &pathsize, 4))
        return NULL;

    int size = foldersize + 1 + pathsize + 1; // / , \0
    uint16_t *buf = (uint16_t*) malloc(size * 2);
    char *songpath = (char*) malloc(size);

    if (!buf || !songpath)
        goto readfail;

    if (!readmemory(&st, ptr_add(folder_ptr, 8), buf, foldersize * 2))
        goto readfail;

    buf[foldersize] = '/';

    if (!readmemory(&st, ptr_add(path_ptr, 8), buf + foldersize + 1, pathsize * 2))
        goto readfail;

    int i;
    for (i = 0; i < size - 1; i++)
    {
        if (*(buf+i) > 127) goto readfail;

        if (*(buf+i) == '\\') *(songpath+i) = '/'; // workaround: some installation put \ in song folder
        else *(songpath+i) = *(buf+i);
    }

    *(songpath+i) = '\0';
    free(buf);
    *length = size;
    return songpath;

readfail:
    free(songpath);
    free(buf);
    return NULL;
} // may change it to wchar_t since path may contain unicode characters, but i will just use char here for now to keep things simple

volatile int run = 1;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void gotquitsig(int sig)
{
    run = 0;
}
#pragma GCC diagnostic pop

int main()
{
    setbuf(stdout, NULL);
    ptr_type base = NULL;

    char *songpath = NULL;
    char *oldpath = NULL;
    unsigned int len = 0;

    int fd = open("/tmp/osu_path", O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if (fd == -1)
    {
        perror("/tmp/osu_path");
        return -1;
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = gotquitsig;
    if (sigaction(SIGINT, &act, NULL) != 0 && sigaction(SIGTERM, &act, NULL) != 0)
    {
        perror(NULL);
        return -3;
    }

    while (run)
    {
        if (OSUMEM_NOT_FOUND(&st))
        {
            find_and_set_osu(&st);
            if (OSUMEM_NOT_FOUND(&st))
            {
                putchar('.');
                goto contin;
            }
            else
            {
                putchar('\n');
            }
        }
        else
        {
            find_and_set_osu(&st);
        }

        if (OSUMEM_OK(&st))
        {
            if (OSUMEM_NEW_OSU(&st))
            {
                fputs("osu! is found. ", stdout);
                if (init_memread(&st) == -1)
                {
                    puts("failed opening memory...");
                    continue;
                }
            }

            if (base == NULL)
            {
                puts("starting to scan memory...");
                base = match_pattern();
                if (base != NULL)
                {
                    puts("scan succeeded. you can now use 'auto' option");
                }
                else
                {
                    printerr("failed scanning memory: it could be because it's too early");
                    printerr("will retry in 3 seconds...");
                    sleep(3);
                    continue;
                }
            }

            songpath = get_mappath(base, &len);
            if (songpath != NULL)
            {
                if (oldpath != NULL && strcmp(songpath, oldpath) == 0)
                {
                    free(songpath);
                    goto contin;
                }
                else
                {
                    free(oldpath);
                    oldpath = songpath;
                }
            }
            else
            {
                printerr("failed reading memory: scanning again...");
                base = NULL;
                goto contin;
            }

            if (lseek(fd, 0, SEEK_SET) == -1)
            {
                perror("/tmp/osu_path");
            }
            else
            {
                ssize_t w = write(fd, songpath, len);
                if (w == -1 || ftruncate(fd, w) == -1)
                {
                    perror("/tmp/osu_path");
                }
            }
        }
        else
        {
            fputs("process lost! waiting for osu", stderr);
            base = NULL;
            stop_memread(&st);
        }
contin:
        sleep(1);
    }
    unlink("/tmp/osu_path");
    free(oldpath);
    stop_memread(&st);
    close(fd);
    return 0;
}
