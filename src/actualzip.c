#include <zip.h>
#include "actualzip.h"
#include "tools.h"
#include "cosuplatform.h"

// need to move on to another library due to lack of unicode support in minizip
int create_actual_zip(char *zipfile, struct buffers *bufs)
{
    char *audname = strrchr(bufs->audpath, PATHSEP) + 1;
    char *mapname = strrchr(bufs->mappath, PATHSEP) + 1;

    zip_t *zf = zip_open(zipfile, ZIP_CREATE, NULL);
    if (zf == NULL)
    {
        printerr("Failed creating a zip file");
        return 1;
    }

    zip_source_t *audb = NULL;
    if (bufs->audpath)
    {
        audb = zip_source_buffer(zf, bufs->audbuf, bufs->audlast, 0);
        if (audb == NULL)
        {
            zip_discard(zf);
            return 1;
        }
        else
        {
            if (zip_file_add(zf, audname, audb, ZIP_FL_ENC_UTF_8) == -1)
            {
                zip_source_free(audb);
                zip_discard(zf);
                return 1;
            }
        }
    }

    zip_source_t *mapb = zip_source_buffer(zf, bufs->mapbuf, bufs->maplast, 0);
    if (mapb == NULL)
    {
        if (audb) zip_source_free(audb);
        zip_discard(zf);
        return 1;
    }
    else
    {
        if (zip_file_add(zf, mapname, mapb, ZIP_FL_ENC_UTF_8) == -1)
        {
            if (audb) zip_source_free(audb);
            zip_source_free(mapb);
            zip_discard(zf);
            return 1;
        }
    }

    if (zip_close(zf) == -1)
    {
        printerr("Failed finishing a zip file");
        zip_discard(zf);
        return 1;
    }
    return 0;
}
