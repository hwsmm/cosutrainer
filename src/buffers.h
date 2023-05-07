#pragma once
#include <sys/types.h>

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

    char *mappath;
    char *audpath;
};

int buffers_init(struct buffers *bufs);
void buffers_free(struct buffers *bufs);

#define BUFFERS_METHODS(BUF) \
int buffers_##BUF##_resize(struct buffers *bufs, size_t size); \
int buffers_##BUF##_put(struct buffers *bufs, const void *data, size_t size); \
int buffers_##BUF##_get(struct buffers *bufs, void *data, size_t size); \
ssize_t buffers_##BUF##_seek(struct buffers *bufs, ssize_t offset, int whence);

BUFFERS_METHODS(map)

BUFFERS_METHODS(aud)

#ifdef __cplusplus
}
#endif
