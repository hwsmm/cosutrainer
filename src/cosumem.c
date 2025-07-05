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
#include <ctype.h>
#include <stdbool.h>
#include <locale.h>
#include "cosumem.h"
#include "tools.h"
#include "cosuplatform.h"
#include "winregread.h"
#include "sigscan.h"

bool match_pattern(struct sigscan_status *st, ptr_type *baseaddr)
{
    if (baseaddr != NULL && *baseaddr == PTR_NULL)
    {
        const uint8_t ptrn[] = OSU_BASE_SIG;
        *baseaddr = find_pattern(st, ptrn, OSU_BASE_SIZE, NULL);
    }
    return (baseaddr == NULL || *baseaddr != PTR_NULL);
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
}

#ifndef WIN32
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
    fprintf(stderr, "Current locale: %s\n", setlocale(LC_CTYPE, ""));
    struct sigscan_status st;
    init_sigstatus(&st);
    ptr_type base = PTR_NULL;

    wchar_t *songpath = NULL;
    wchar_t *oldpath = NULL;
    unsigned int len = 0;

    char *songsfd = NULL;
    int songsfdlen = 0;

    int fd = -1;

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = gotquitsig;
    if (sigaction(SIGINT, &act, NULL) != 0 && sigaction(SIGTERM, &act, NULL) != 0)
    {
        perror(NULL);
        return -3;
    }

    printerr("Memory scanner is now starting... Open osu! and get into song select!");
    while (run)
    {
        DEFAULT_LOGIC(&st,
        {
            char *disable_env = getenv("OSUMEM_DISABLE_FOLDER_DETECT");
            if (disable_env != NULL && *disable_env == 'y')
            {
                printerr("osu! is found! Skipping song folder detection...");
                goto skip;
            }

            printerr("osu! is found, Now looking for its song folder...");
            char envf[1024];
            snprintf(envf, 1024, "/proc/%d/environ", st.osu);

            int size = 0;
            char *envs = read_file(envf, &size);
            char *pfx = NULL;
            const char name[] = "WINEPREFIX=";

            if (envs != NULL)
            {
                char *current = envs;
                while ((intptr_t)current - (intptr_t)envs < size)
                {
                    if (strncmp(current, name, sizeof(name) - 1) != 0)
                    {
                        current += strlen(current) + 1;
                        continue;
                    }

                    current += sizeof(name) - 1;
                    pfx = current;
                    break;
                }
            }

            if (pfx != NULL)
                fprintf(stderr, "Found WINEPREFIX: %s\n", pfx);
            else
                fprintf(stderr, "WINEPREFIX is not found, falling back to default prefix...\n");

            char uid[1024];
            snprintf(uid, sizeof(uid), "/proc/%d/status", st.osu);
            FILE *idfd = fopen(uid, "r");

            if (idfd == NULL)
            {
                printerr("Failed getting UID that is running osu");
            }
            else
            {
                char *uidptr = NULL;
                while (fgets(uid, sizeof(uid), idfd))
                {
                    const char uid_key[] = "Uid:";
                    if (strncmp(uid, uid_key, sizeof(uid_key) - 1) == 0)
                    {
                        int uidtemp = -1;
                        if (sscanf(uid, "Uid: %d", &uidtemp) > 0 && snprintf(uid, sizeof(uid), "%d", uidtemp) > 0)
                        {
                            uidptr = uid;
                            break;
                        }
                    }
                }
                fclose(idfd);

                if (uidptr != NULL)
                {
                    songsfd = get_osu_songs_path(pfx, uidptr);
                    if (songsfd != NULL)
                    {
                        fprintf(stderr, "Found Song folder: %s\n", songsfd);
                        songsfdlen = strlen(songsfd);
                    }
                }
                else
                {
                    printerr("Couldn't find UID of a process");
                }
            }

            free(envs);
        },
        {
skip:
            if (base == PTR_NULL)
            {
                printerr("Starting to scan memory...");

                if (match_pattern(&st, &base))
                {
                    printerr("Scan succeeded. cosu-trainer will now update map information.");
                }
                else
                {
                    printerr("Failed scanning memory: It could be because it's too early");
                    printerr("Will retry in 3 seconds...");
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
                printerr("Failed reading memory: Scanning again...");
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
                int write = songsfd != NULL
                            ? dprintf(fd, "%d %s/%ls", songsfdlen, songsfd, songpath)
                            : dprintf(fd, "0 %ls", songpath);

                if (write < 0)
                    perror("dprintf");
                else if (ftruncate(fd, write) == -1)
                    perror("truncate");
            }
        },
        {
            printerr("Process lost! Waiting for osu...");
            base = NULL;

            free(songsfd);
            songsfd = NULL;
        });
contin:
        sleep(1);
    }

    unlink("/tmp/osu_path");
    if (songpath != oldpath) free(oldpath);
    if (songpath != NULL) free(songpath);
    free(songsfd);
    stop_memread(&st);
    close(fd);
    return 0;
}
#endif
