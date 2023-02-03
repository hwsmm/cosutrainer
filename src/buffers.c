#include <string.h>
#include <stdlib.h>
#include "buffers.h"
#include "tools.h"

int buffers_init(struct buffers *bufs)
{
   bufs->mapbuf = (char*) malloc(MAPFILE_INITSIZE);
   if (bufs->mapbuf == NULL)
   {
      printerr("Failed initalizing buffers");
      return 1;
   }
   bufs->mapsize = MAPFILE_INITSIZE;
   bufs->mapcur = 0;
   bufs->maplast = 0;
   bufs->mapname = NULL;

   bufs->audbuf = (char*) malloc(AUDFILE_INITSIZE);
   if (bufs->audbuf == NULL)
   {
      printerr("Failed initalizing buffers");
      return 1;
   }
   bufs->audsize = AUDFILE_INITSIZE;
   bufs->audcur = 0;
   bufs->audlast = 0;
   bufs->audname = NULL;

   return 0;
}

void buffers_free(struct buffers *bufs)
{
   free(bufs->mapbuf);
   free(bufs->audbuf);
   free(bufs->mapname);
   free(bufs->audname);
}

#define DEFINE_BUFFERS_RESIZE(BUF) \
int buffers_##BUF##_resize(struct buffers *bufs, size_t size) \
{ \
   char *reall = (char*) realloc(bufs->BUF##buf, size); \
   if (reall == NULL) \
   { \
      printerr("Failed reallocating memory"); \
      return -1; \
   } \
   bufs->BUF##buf = reall; \
   bufs->BUF##size = size; \
   return 0; \
}

#define DEFINE_BUFFERS_PUT(BUF) \
int buffers_##BUF##_put(struct buffers *bufs, const void *data, size_t size) \
{ \
   if (bufs->BUF##size - bufs->BUF##cur - 1 < size) \
   { \
      size_t newsize = (size > BUF##_reallstep ? size : 0) + bufs->BUF##size + BUF##_reallstep; \
      if (buffers_##BUF##_resize(bufs, newsize) == -1) \
      { \
         printerr("Failed reallocating memory"); \
         return 1; \
      } \
   } \
   memcpy(bufs->BUF##buf + bufs->BUF##cur, data, size); \
   bufs->BUF##cur += size; \
   if (bufs->BUF##cur > bufs->BUF##last) bufs->BUF##last = bufs->BUF##cur; \
   return 0; \
}

#define DEFINE_BUFFERS_GET(BUF) \
int buffers_##BUF##_get(struct buffers *bufs, void *data, size_t size) \
{ \
   size_t read = size; \
   if (bufs->BUF##size < bufs->BUF##cur + size) \
   { \
      read = bufs->BUF##size - bufs->BUF##cur; \
   } \
   memcpy(data, bufs->BUF##buf + bufs->BUF##cur, read); \
   return read; \
}

#define DEFINE_BUFFERS_SEEK(BUF) \
ssize_t buffers_##BUF##_seek(struct buffers *bufs, ssize_t offset, int whence) \
{ \
   if (whence == SEEK_SET) \
   { \
      bufs->BUF##cur = offset; \
   } \
   else if (whence == SEEK_CUR) \
   { \
      bufs->BUF##cur += offset; \
   } \
   else if (whence == SEEK_END) \
   { \
      bufs->BUF##cur = bufs->BUF##last + offset; \
   } \
   return bufs->BUF##cur; \
}

#define DEFINE_BUFFERS(BUF) \
DEFINE_BUFFERS_RESIZE(BUF) \
DEFINE_BUFFERS_PUT(BUF) \
DEFINE_BUFFERS_GET(BUF) \
DEFINE_BUFFERS_SEEK(BUF)

DEFINE_BUFFERS(map)

DEFINE_BUFFERS(aud)
