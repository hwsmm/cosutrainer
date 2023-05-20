#include <zip.h>
#include "actualzip.h"
#include "tools.h"
#include "cosuplatform.h"

int create_actual_zip(char *zipfile, struct buffers *bufs)
{
    zip_t *zf = zip_open(zipfile, ZIP_CREATE, NULL);
    if (zf == NULL)
    {
        printerr("Failed creating a zip file");
        return 1;
    }

    zip_source_t *audb = NULL;
    if (bufs->audname)
    {
        audb = zip_source_buffer(zf, bufs->audbuf, bufs->audlast, 0);
        if (audb == NULL)
        {
            zip_discard(zf);
            return 1;
        }
        else
        {
            if (zip_set_file_compression(zf, zip_file_add(zf, bufs->audname, audb, ZIP_FL_ENC_UTF_8), ZIP_CM_STORE, 0) == -1)
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
        if (zip_set_file_compression(zf, zip_file_add(zf, bufs->mapname, mapb, ZIP_FL_ENC_UTF_8), ZIP_CM_STORE, 0) == -1)
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
