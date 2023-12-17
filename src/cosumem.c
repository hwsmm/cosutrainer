#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <wchar.h>
#include <wctype.h>
#include <stdbool.h>
#include <locale.h>
#include "cosumem.h"
#include "tools.h"
#include "cosuplatform.h"

extern ptr_type find_pattern(struct sigscan_status *st, const uint8_t bytearray[], const unsigned int pattern_size, const bool mask[]);

bool match_pattern(struct sigscan_status *st, ptr_type *baseaddr)
{
    if (baseaddr != NULL && *baseaddr == PTR_NULL) _osu_find_ptrn(*baseaddr, st, BASE);
    return (baseaddr == NULL || *baseaddr != PTR_NULL);
}

// deprecated
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
    if (!readmemory(st, ptr_add(get_beatmap_ptr(st, base_address), 0xC8), &id, 4))
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

    if (!readmemory(st, ptr_add(beatmap_ptr, 0x90), &path_ptr, PTR_SIZE))
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

    if (!readmemory(st, ptr_add(path_ptr, 8), pathstrbuf, pathsize * 2))
        goto readfail;

    *(pathstrbuf+pathsize) = '\0';

    int size;
    size = foldersize + 1 + pathsize + 1; // / , \0
    wchar_t *songpath;
    songpath = (wchar_t*) malloc(size * sizeof(wchar_t));

    if (!songpath) goto readfail;

    int i;
    for (i = 0; i < size - 1; i++)
    {
        uint16_t put;
        if (i < foldersize) put = *(folderstrbuf + i);
        else if (i == foldersize) put = PATHSEP;
        else put = *(pathstrbuf + (i - foldersize - 1));
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
    fprintf(stderr, "Current locale: %s\n", setlocale(LC_CTYPE, ""));
    struct sigscan_status st;
    init_sigstatus(&st);
    ptr_type base = PTR_NULL;

    wchar_t *songpath = NULL;
    wchar_t *oldpath = NULL;
    unsigned int len = 0;

    int fd = -1;

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = gotquitsig;
    if (sigaction(SIGINT, &act, NULL) != 0 && sigaction(SIGTERM, &act, NULL) != 0)
    {
        perror(NULL);
        return -3;
    }

    printerr("memory scanner is starting... open osu! if you didn't");
    while (run)
    {
        DEFAULT_LOGIC(&st,
        {
            printerr("osu is found!");
            char envf[1024];
            snprintf(envf, 1024, "/proc/%d/environ", st.osu);

            int fpd = open(envf, O_RDONLY);
            int efd = open("/tmp/osu_wine_env", O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
            if (fpd == -1 || efd == -1)
            {
                perror("env_read");
            }
            else
            {
                ssize_t rd = 1;
                char buf;
                bool firsteq = true;
                bool last = false;
                bool again = false;
                while (again || (rd = read(fpd, &buf, 1)) == 1)
                {
                    if (buf == '\0')
                    {
                        if (firsteq == false) last = true;
                        firsteq = true;
                        buf = '"';
                    }
                    if (write(efd, &buf, sizeof(char)) == -1)
                    {
                        perror("/tmp/osu_wine_env");
                        unlink("/tmp/osu_wine_env");
                        break;
                    }
                    if (again) again = false;
                    if (firsteq && buf == '=')
                    {
                        firsteq = false;
                        buf = '"';
                        again = true;
                    }
                    if (last && buf == '"')
                    {
                        last = false;
                        buf = ' ';
                        again = true;
                    }
                }
                if (rd < 0)
                {
                    perror(envf);
                }

                snprintf(envf, 1024, "/proc/%d/exe", st.osu);
                char *wine_exe = realpath(envf, NULL);
                if (wine_exe != NULL)
                {
                    char *name = strrchr(wine_exe, '/');
                    if (name != NULL)
                    {
                        name++;
                        if (strcmp(name, "wine-preloader") == 0 || strcmp(name, "wine") == 0)
                        {
                            *(name + 4) = '\0';
                            dprintf(efd, " WINE_EXE=\"%s\"", wine_exe);
                        }
                    }
                    free(wine_exe);
                }
            }
            if (fpd != -1) close(fpd);
            if (efd != -1) close(efd);
        },
        {
            if (base == PTR_NULL)
            {
                printerr("starting to scan memory...");

                if (match_pattern(&st, &base))
                {
                    printerr("scan succeeded. you can now use 'auto' option");
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
                    songpath = NULL;
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

            if (fd == -1)
            {
                fd = open("/tmp/osu_path", O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
                if (fd == -1)
                {
                    perror("/tmp/osu_path");
                    goto contin;
                }
            }

            if (lseek(fd, 0, SEEK_SET) == -1)
            {
                perror("/tmp/osu_path");
            }
            else
            {
                int write = 0;
                if ((write = dprintf(fd, "%d %ls", st.osu, songpath)) < 0)
                {
                    perror("dprintf");
                }
                else
                {
                    if (ftruncate(fd, write) == -1)
                    {
                        perror("truncate");
                    }
                }
            }
        },
        {
            printerr("process lost! waiting for osu...");
            base = NULL;
        })
contin:
        sleep(1);
    }
    unlink("/tmp/osu_path");
    unlink("/tmp/osu_wine_env");
    free(oldpath);
    if (songpath != NULL) free(songpath);
    stop_memread(&st);
    close(fd);
    return 0;
}
#endif
