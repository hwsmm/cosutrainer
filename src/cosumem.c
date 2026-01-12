#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
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

#ifndef WIN32
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#endif

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
    if (size <= 0)
        goto readfail;

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
static void gotsig(int sig)
{
    if (sig != SIGRTMIN)
        run = 0;
}
#pragma GCC diagnostic pop

int main(int argc, char *argv[])
{
    fprintf(stderr, "Current locale: %s\n", setlocale(LC_CTYPE, ""));
    struct sigscan_status st;
    init_sigstatus(&st);
    ptr_type base = PTR_NULL;

    char *songsfd = NULL;

    int fd = msgget(COSU_IPCKEY, IPC_CREAT | S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1)
    {
        perror("msgget");
        return -1;
    }

    struct msqid_ds fdattr = { 0, };
    if (msgctl(fd, IPC_STAT, &fdattr) == -1)
    {
        perror("IPC_STAT");
        return -1;
    }

    if (argc > 1 && strcmp(argv[1], "--max-queue-size") == 0)
    {
        if (argc < 3)
        {
            printerr("Please set the queue size");
            return 1;
        }

        msglen_t qsize = atol(argv[2]);
        fdattr.msg_qbytes = qsize;

        if (msgctl(fd, IPC_SET, &fdattr) == -1)
        {
            perror("IPC_SET");
            return -1;
        }

        fprintf(stderr, "Successfully overrode queue size to %ld.\n", qsize);
    }

    char *msgbuf = (char*)malloc(fdattr.msg_qbytes);
    if (msgbuf == NULL)
    {
        printerr("Failed allocation");
        return -2;
    }
    msglen_t msgbufsize = fdattr.msg_qbytes;

    struct sigaction act = { 0, };
    act.sa_handler = gotsig;
    if (sigaction(SIGINT, &act, NULL) != 0 || sigaction(SIGTERM, &act, NULL) != 0 || sigaction(SIGRTMIN, &act, NULL) != 0)
    {
        perror(NULL);
        return -3;
    }

    struct sigevent sigevt = { 0, };
    sigevt.sigev_notify = SIGEV_SIGNAL;
    sigevt.sigev_signo = SIGRTMIN;
    sigevt.sigev_value.sival_ptr = NULL;

    struct itimerspec timerspec;
    timerspec.it_value.tv_sec = 1;
    timerspec.it_value.tv_nsec = 0;
    timerspec.it_interval.tv_sec = 1;
    timerspec.it_interval.tv_nsec = 0;

    timer_t timerId = 0;
    if (timer_create(CLOCK_REALTIME, &sigevt, &timerId) != 0 || timer_settime(timerId, 0, &timerspec, NULL) != 0)
    {
        perror("timer");
        return -3;
    }

    ssize_t recvbyte = -1;

    printerr("Memory scanner is now starting... Open osu! and get into song select!");

    do
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
                    printerr("Will retry in a bit...");
                    goto contin;
                }
            }

            if (recvbyte < 0)
            {
                goto contin;
            }

            unsigned int len = 0;
            wchar_t *songpath = get_mappath(&st, base, &len);

            if (songpath == NULL)
            {
                printerr("Failed reading memory: Scanning again...");
                base = PTR_NULL;
                goto contin;
            }

            int writ = songsfd != NULL
                       ? snprintf(msgbuf + sizeof(long), msgbufsize - sizeof(long), "%s\n%ls", songsfd, songpath)
                       : snprintf(msgbuf + sizeof(long), msgbufsize - sizeof(long), "%ls", songpath);

            *(long*)msgbuf = COSU_IPCPATH; // mtype

            if (writ < 0)
            {
                perror(NULL);
            }
            else if (msgsnd(fd, msgbuf, writ + 1, IPC_NOWAIT) == -1)
            {
                perror("msgsnd");

                fprintf(stderr, "Your message queue buffer might be too small for this map path.\n"
                                "Consider running `%s --max-queue-size %lu` to increase limit."
                                "You may need to restart cosu-trainer after running the command.\n"
                                "However, you should rather place your osu install in a better location if your song folder path is too long.", argv[0], msgbufsize * 2);
            }

            free(songpath);
        },
        {
            printerr("Process lost! Waiting for osu...");
            base = PTR_NULL;

            free(songsfd);
            songsfd = NULL;
        });
contin:
        recvbyte = msgrcv(fd, msgbuf, msgbufsize, COSU_IPCREQ, 0);
        if (recvbyte < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("msgrcv");
                break;
            }
        }
    }
    while (run);

    free(songsfd);
    free(msgbuf);
    stop_memread(&st);
    timer_delete(timerId);
    msgctl(fd, IPC_RMID, NULL);
    return 0;
}
#endif
