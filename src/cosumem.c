#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/stat.h>
#include <wchar.h>
#include "cosumem.h"
#include "tools.h"
#include "cosuplatform.h"

static uint16_t *trim(uint16_t *str, int *res_size)
{
    uint16_t *end = str + *res_size;
    while (isspace(*str) && *str != '\0')
    {
        str++;
        (*res_size)--;
    }

    while (isspace(*(--end)) && end >= str)
    {
        (*res_size)--;
    }
    *(end + 1) = '\0';

    return str;
}

char *get_songsfolder(struct sigscan_status *st)
{
    char *expath = get_rootpath(st);
    if (expath == NULL)
    {
        printerr("Failed getting process path!");
        return NULL;
    }

    int pathsize = strlen(expath) + 1 + 5 + 1;
    char *songspath = (char*) calloc(pathsize, sizeof(char)); // "/Songs"
    if (songspath == NULL)
    {
        printerr("Failed allocation of wide string of song folder!");
        return NULL;
    }
    snprintf(songspath, pathsize, "%s" STR_PATHSEP "Songs", expath);
    free(expath);

    struct stat stt;
    if (stat(songspath, &stt) != 0)
    {
        perror("Songs folder");
        free(songspath);
        return NULL;
    }

    if (stt.st_mode & S_IFDIR)
    {
        return songspath;
    }
    else
    {
        printerr("Couldn't find the song folder! Set OSU_SONG_FOLDER");
        free(songspath);
        return NULL;
    }
}

ptr_type match_pattern(struct sigscan_status *st)
{
    // "F8 01 74 04 83 65"
    const uint8_t basepattern[] = { 0xf8, 0x01, 0x74, 0x04, 0x83, 0x65 };
    return find_pattern(st, basepattern, 6, NULL);
}

ptr_type get_beatmap_ptr(struct sigscan_status *st, ptr_type base_address)
{
    ptr_type beatmap_2ptr = PTR_NULL;
    ptr_type beatmap_ptr = PTR_NULL;

    if (!readmemory(st, ptr_add(base_address, -0xC), &beatmap_2ptr, PTR_SIZE))
        return PTR_NULL;

    if (!readmemory(st, beatmap_2ptr, &beatmap_ptr, PTR_SIZE))
        return PTR_NULL;

    return beatmap_ptr;
}

int get_mapid(struct sigscan_status *st, ptr_type base_address)
{
    int id = 0;
    if (!readmemory(st, ptr_add(get_beatmap_ptr(st, base_address), 0xCC), &id, 4))
        return -1;

    return id;
}

wchar_t *get_mappath(struct sigscan_status *st, ptr_type base_address, unsigned int *length)
{
    ptr_type beatmap_ptr = get_beatmap_ptr(st, base_address);
    if (beatmap_ptr == PTR_NULL) return PTR_NULL;

    ptr_type folder_ptr = PTR_NULL;
    ptr_type path_ptr = PTR_NULL;
    int foldersize = 0;
    int pathsize = 0;

    if (!readmemory(st, ptr_add(beatmap_ptr, 0x78), &folder_ptr, PTR_SIZE))
        return NULL;

    if (!readmemory(st, ptr_add(folder_ptr, 4), &foldersize, 4))
        return NULL;

    if (!readmemory(st, ptr_add(beatmap_ptr, 0x94), &path_ptr, PTR_SIZE))
        return NULL;

    if (!readmemory(st, ptr_add(path_ptr, 4), &pathsize, 4))
        return NULL;

    uint16_t *folderstrbuf = (uint16_t*) malloc((foldersize + 1) * 2);
    uint16_t *pathstrbuf = (uint16_t*) malloc((pathsize + 1) * 2);

    if (!folderstrbuf || !pathstrbuf)
        goto readfail;

    if (!readmemory(st, ptr_add(folder_ptr, 8), folderstrbuf, foldersize * 2))
        goto readfail;

    *(folderstrbuf+foldersize) = '\0';
    uint16_t *trim_fdstr;
    trim_fdstr = trim(folderstrbuf, &foldersize);

    if (!readmemory(st, ptr_add(path_ptr, 8), pathstrbuf, pathsize * 2))
        goto readfail;

    *(pathstrbuf+pathsize) = '\0';
    uint16_t *trim_pstr;
    trim_pstr = trim(pathstrbuf, &pathsize);

    int size;
    size = foldersize + 1 + pathsize + 1; // / , \0
    wchar_t *songpath;
    songpath = (wchar_t*) malloc(size * sizeof(wchar_t));

    if (!songpath) goto readfail;

    int i;
    for (i = 0; i < size - 1; i++)
    {
        uint16_t put;
        if (i < foldersize) put = *(trim_fdstr + i);
        else if (i == foldersize) put = '/';
        else put = *(trim_pstr + (i - foldersize - 1));
#ifndef WIN32
        if (put == '\\') put = '/'; // workaround: some installation put \ in song folder
#endif
        *(songpath+i) = put;
    }

    *(songpath+i) = '\0';
    free(folderstrbuf);
    free(pathstrbuf);
    *length = size;
    return songpath;

readfail:
    free(folderstrbuf);
    free(pathstrbuf);
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

#ifndef WIN32
int main()
{
    struct sigscan_status st;
    init_sigstatus(&st);
    setbuf(stdout, NULL);
    ptr_type base = NULL;

    wchar_t *songpath = NULL;
    wchar_t *oldpath = NULL;
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
                base = match_pattern(&st);
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

            songpath = get_mappath(&st, base, &len);
            if (songpath != NULL)
            {
                if (oldpath != NULL && wcscmp(songpath, oldpath) == 0)
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
                char *mbstr = (char*) malloc(len * MB_CUR_MAX);
                if (mbstr == NULL)
                {
                    printerr("Failed allocation!");
                }
                else
                {
                    size_t convbytes = wcstombs(mbstr, songpath, len * MB_CUR_MAX);
                    if (convbytes != -1)
                    {
                        ssize_t w = write(fd, mbstr, convbytes);
                        if (w == -1 || ftruncate(fd, w) == -1)
                        {
                            perror("/tmp/osu_path");
                        }
                    }
                    else
                    {
                        printerr("Failed converting!");
                    }
                    free(mbstr);
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
    free(songpath);
    stop_memread(&st);
    close(fd);
    return 0;
}
#endif
