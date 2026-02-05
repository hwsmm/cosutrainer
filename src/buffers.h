#pragma once
#include <stddef.h>
#ifndef WIN32
#include <sys/types.h>
#else
#include <BaseTsd.h>
#define ssize_t SSIZE_T
#endif

#define MAPFILE_INITSIZE 8192
#define AUDFILE_INITSIZE 3145728

#define map_reallstep 8192
#define aud_reallstep 524288

#ifdef __cplusplus
extern "C"
{
#endif

struct buffers
{
    char *mapbuf;
    char *audbuf;

    size_t mapsize;
    size_t audsize;

    unsigned long mapcur;
    unsigned long audcur;

    unsigned long maplast;
    unsigned long audlast;

    char *mapname;
    char *audname;
};

int buffers_init(struct buffers *bufs);
void buffers_free(struct buffers *bufs);

int buffers_map_resize(struct buffers *bufs, size_t size);
int buffers_map_put(struct buffers *bufs, const void *data, size_t size);
int buffers_map_get(struct buffers *bufs, void *data, size_t size);
ssize_t buffers_map_seek(struct buffers *bufs, ssize_t offset, int whence);
void buffers_map_reset(struct buffers *bufs);

int buffers_aud_resize(struct buffers *bufs, size_t size);
int buffers_aud_put(struct buffers *bufs, const void *data, size_t size);
int buffers_aud_get(struct buffers *bufs, void *data, size_t size);
ssize_t buffers_aud_seek(struct buffers *bufs, ssize_t offset, int whence);
void buffers_aud_reset(struct buffers *bufs);

#ifdef __cplusplus
}
#endif
