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

DEFINE_EXTERN_SIGSCAN_FUNCTIONS;

bool match_pattern(struct sigscan_status *st, ptr_type *baseaddr)
{
    if (baseaddr != NULL && *baseaddr == PTR_NULL) _osu_find_ptrn(*baseaddr, st, BASE);
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
static char *get_osu_path(char *wineprefix)
{
    const char system_reg[] = "system.reg";
    int size = strlen(wineprefix) + 1 + sizeof(system_reg);
    char *regpath = (char*) malloc(size);
    if (regpath == NULL)
    {
        printerr("Failed allocating memory!");
        return NULL;
    }

    snprintf(regpath, size, "%s/%s", wineprefix, system_reg);

    FILE *f = fopen(regpath, "r");
    if (!f)
    {
        perror(regpath);
        return NULL;
    }

    char line[4096];
    char *result = NULL;

    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "osu\\\\shell\\\\open\\\\command") != NULL)
        {
            while (fgets(line, sizeof(line), f))
            {
                char *find = NULL;
                if ((find = strstr(line, "osu!.exe")) != NULL)
                {
                    *find = '\0';

                    char *first = strstr(line, ":\\\\");

                    if (first != NULL)
                        first--;
                    else
                        goto exit;

                    int len = strlen(first);
                    result = (char*) malloc(len);
                    if (result != NULL)
                        strcpy(result, first);

                    goto exit;
                }
            }
        }
    }

exit:
    fclose(f);
    return result;
}

static char *get_song_path(char *wineprefix, char *uid)
{
    FILE *pw = fopen("/etc/passwd", "r");
    if (!pw)
    {
        perror("/etc/passwd");
        return NULL;
    }

    char id[4096];
    bool uidfound = false;
    bool prefixnull = wineprefix == NULL;

    while (fgets(id, sizeof(id), pw))
    {
        /*char *lid =*/ strtok(id, ":");
        /*char *lpw =*/ strtok(NULL, ":");
        char *luid = strtok(NULL, ":");

        if (strcmp(uid, luid) == 0)
        {
            uidfound = true;

            if (prefixnull)
            {
                /*char *lgid =*/ strtok(NULL, ":");
                /*char *lgecos =*/ strtok(NULL, ":");
                char *ldir = strtok(NULL, ":");

                const char winedefp[] = ".wine";

                int defplen = strlen(ldir) + 1 + sizeof(winedefp);
                wineprefix = (char*) malloc(defplen);

                if (!wineprefix)
                {
                    fclose(pw);
                    return NULL;
                }
                else
                {
                    snprintf(wineprefix, defplen, "%s/%s", ldir, winedefp);
                }
            }

            break;
        }
    }

    fclose(pw);

    if (!uidfound)
        return NULL;

    char *result = NULL;

    int prefixlen = strlen(wineprefix);

    char *osu = get_osu_path(wineprefix);
    if (osu == NULL)
        return NULL;

    int osulen = strlen(osu);

    for (int i = 0; i < osulen; i++)
    {
        if (*(osu + i) == '\\')
            *(osu + i) = '/';
    }

    *osu = tolower(*osu); // device letter

    const char dosd[] = "dosdevices";
    const char prefix[] = "osu!.";
    const char suffix[] = ".cfg";

    int idlen = strlen(id);
    int cfglen = prefixlen + 1 + (sizeof(dosd)-1) + 1 + osulen + 1 + (sizeof(prefix)-1) + idlen + (sizeof(suffix)-1) + 1;
    char *cfgpath = (char*) malloc(cfglen);

    snprintf(cfgpath, cfglen, "%s/%s/%s/%s%s%s", wineprefix, dosd, osu, prefix, id, suffix);

    if (try_convertwinpath(cfgpath, prefixlen + 1 + (sizeof(dosd)-1) + 1) < 0)
    {
        fprintf(stderr, "Tried config path but failed: %s\n", cfgpath);
        goto freeq;
    }

    FILE* cfg = fopen(cfgpath, "r");
    if (!cfg)
    {
        perror(cfgpath);
        goto freeq;
    }

    char line[4096];
    char find[] = "BeatmapDirectory = ";
    char *found = NULL;
    while (fgets(line, sizeof(line), cfg))
    {
        if (strncmp(line, find, sizeof(find) - 1) == 0)
        {
            found = line + sizeof(find) - 1;
            found = trim(found, NULL);
            break;
        }
    }

    fclose(cfg);

    if (found == NULL)
    {
        printerr("BeatmapDirectory not found in osu config!");
        goto freeq;
    }

    int foundlen = strlen(found);
    char *sep = strchr(found, ':');
    bool abs = sep != NULL && sep - found == 1;

    for (int i = 0; i < foundlen; i++)
    {
        if (*(found + i) == '\\')
            *(found + i) = '/';
    }

    if (abs)
    {
        *found = tolower(*found);

        int reslen = prefixlen + 1 + (sizeof(dosd)-1) + 1 + foundlen + 1;
        result = (char*) malloc(reslen);
        if (result == NULL)
        {
            printerr("Failed allocation!");
            goto freeq;
        }

        snprintf(result, reslen, "%s/%s/%s", wineprefix, dosd, found);
    }
    else
    {
        int reslen = prefixlen + 1 + (sizeof(dosd)-1) + 1 + osulen + 1 + foundlen + 1;
        result = (char*) malloc(reslen);
        if (result == NULL)
        {
            printerr("Failed allocation!");
            goto freeq;
        }

        snprintf(result, reslen, "%s/%s/%s/%s", wineprefix, dosd, osu, found);
    }

    if (result != NULL && try_convertwinpath(result, prefixlen + 1 + (sizeof(dosd)-1) + 1) < 0)
    {
        fprintf(stderr, "Tried final path but failed: %s\n", result);
        free(result);
        result = NULL;
    }

    char *real = realpath(result, NULL);
    if (real != NULL)
    {
        free(result);
        result = real;
    }

freeq:
    if (prefixnull)
        free(wineprefix);

    free(osu);
    free(cfgpath);
    return result;
}

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
            printerr("osu! is found, Now looking for its song folder...");
            char envf[1024];
            snprintf(envf, 1024, "/proc/%d/environ", st.osu);

            int fpd = open(envf, O_RDONLY);
            if (fpd == -1)
            {
                perror(envf);
            }
            else
            {
                ssize_t rd = 1;
                char buf;
                char pfx[2048] = { '\0' };
                while ((rd = read(fpd, &buf, 1)) == 1)
                {
                    if (buf == 'W')
                    {
                        char cmp[10] = "INEPREFIX=";
                        char buf2[10];
                        if ((rd = read(fpd, buf2, sizeof(cmp))) >= (ssize_t)sizeof(cmp) && strncmp(cmp, buf2, sizeof(cmp)-1) == 0)
                        {
                            int idx = 0;
                            while ((rd = read(fpd, &buf, 1)) == 1)
                            {
                                if (idx >= (int)sizeof(pfx))
                                {
                                    printerr("WINEPREFIX is too long!");
                                    break;
                                }

                                pfx[idx++] = buf;
                                if (buf == '\0')
                                    break;
                            }
                        }
                    }
                }
                close(fpd);

                if (pfx[0] != '\0')
                    fprintf(stderr, "Found WINEPREFIX: %s\n", pfx);
                else
                    fprintf(stderr, "WINEPREFIX is not found, falling back to default prefix...\n");

                char uid[128];
                snprintf(uid, sizeof(uid), "/proc/%d/loginuid", st.osu);
                int idfd = open(uid, O_RDONLY);
                ssize_t idlen = 0;

                if (idfd == -1 || (idlen = read(idfd, uid, sizeof(uid) - 1)) <= 0)
                {
                    printerr("Failed getting UID that is running osu");
                }
                else
                {
                    uid[idlen] = '\0';
                    songsfd = get_song_path(pfx[0] == '/' ? pfx : NULL, uid);
                    if (songsfd != NULL)
                    {
                        fprintf(stderr, "Found Song folder: %s\n", songsfd);
                        songsfdlen = strlen(songsfd);
                    }
                }
            }
        },
        {
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
    free(oldpath);
    free(songsfd);
    if (songpath != NULL) free(songpath);
    stop_memread(&st);
    close(fd);
    return 0;
}
#endif
