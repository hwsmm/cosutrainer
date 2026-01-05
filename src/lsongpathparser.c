#include "lsongpathparser.h"
#include "tools.h"
#include "cosuplatform.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void songpath_init(struct songpath_status *st)
{
    memset(st, 0, sizeof(struct songpath_status));
    st->fd = -1;
}

void songpath_free(struct songpath_status *st)
{
    if (st->songf) free(st->songf);
    if (st->save) free(st->save);
    if (st->msgbuf) free(st->msgbuf);
    if (st->path) free(st->path);
}

bool songpath_get(struct songpath_status *st, char **path)
{
    if (st->fd == -1)
    {
        st->fd = msgget(COSU_IPCKEY, 0);

        if (st->fd == -1)
        {
            perror(NULL);

            if (st->waitcnt <= 0)
                printerr("Waiting for osumem...");

            st->waitcnt++;

            return false;
        }
        else
        {
            printerr("Found osumem!");
            st->waitcnt = 0;
        }
    }

    if (st->msgbuf == NULL)
    {
        struct msqid_ds fdattr = { 0, };
        if (msgctl(st->fd, IPC_STAT, &fdattr) == -1)
        {
            perror("msgctl");
            return false;
        }

        st->msgbuf = (char*)malloc(fdattr.msg_qbytes);
        if (st->msgbuf == NULL)
        {
            printerr("Failed allocation");
            return false;
        }

        st->msgbufsize = fdattr.msg_qbytes;
    }

    long mtype = COSU_IPCREQ;

    if (msgsnd(st->fd, &mtype, 0, IPC_NOWAIT) == -1)
    {
        printerr("osumem might be lost...");

        st->fd = -1;
        free(st->msgbuf);
        st->msgbuf = NULL;

        perror("msgsnd");
        return false;
    }

    ssize_t readbytes = msgrcv(st->fd, st->msgbuf, st->msgbufsize, COSU_IPCPATH, 0);
    if (readbytes == -1)
    {
        if (errno != EINTR)
        {
            printerr("osumem might be lost...");

            st->fd = -1;
            free(st->msgbuf);
            st->msgbuf = NULL;

            perror("msgrcv");
        }

        return false;
    }

    char *msgbuf = st->msgbuf + sizeof(long);

    if (readbytes == 0)
    {
        return false;
    }

    if (st->save == NULL)
    {
    }
    else if (strcmp(st->save, msgbuf) == 0)
    {
        return false;
    }

    if (st->save == NULL || st->savesize < readbytes)
    {
        free(st->save);
        st->save = (char*)malloc(readbytes);
        if (st->save == NULL)
        {
            printerr("Failed allocation");
            return false;
        }

        st->savesize = readbytes;
    }

    strcpy(st->save, msgbuf);
    char *sep = strchr(msgbuf, '\n');

    free(st->path);
    st->path = NULL;

    if (*msgbuf != '/' && sep == NULL)
    {
        if (st->songf == NULL)
        {
            st->songf = get_songspath(NULL);
            if (st->songf == NULL)
            {
                printerr("Song Folder not found!");
                return false;
            }
        }

        int songflen = strlen(st->songf);
        int fullsize = songflen + 1 + strlen(msgbuf) + 1;

        st->path = (char*) malloc(fullsize);
        if (st->path == NULL)
        {
            printerr("Failed allocating!");
            return false;
        }

        snprintf(st->path, fullsize, "%s/%s", st->songf, msgbuf);

        if (try_convertwinpath(st->path, songflen + 1) < 0)
        {
            printerr("Failed finding path!");
            free(st->path);
            st->path = NULL;
            return false;
        }

        *path = st->path;
    }
    else if (sep != NULL)
    {
        int songflen = (int)(sep - msgbuf);
        *sep = '/';

        if (try_convertwinpath(msgbuf, songflen + 1) < 0)
        {
            printerr("Failed finding path!");
            return false;
        }

        *path = msgbuf;
    }
    else
    {
        printerr("Malformed message");
        return false;
    }

    return true;
}
