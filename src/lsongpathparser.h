#pragma once
#include <stdbool.h>

#ifndef WIN32

#include <sys/msg.h>
#include <sys/ipc.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct songpath_status
{
    int waitcnt;

    int fd;
    char *msgbuf;
    long msgbufsize;

    char *save;
    ssize_t savesize;

    char *songf;
    char *path;
};

void songpath_init(struct songpath_status *st);
void songpath_free(struct songpath_status *st);
bool songpath_get(struct songpath_status *st, char **path);

#ifdef __cplusplus
}
#endif

#endif
