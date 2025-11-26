#include <minizip/zip.h>
#include <time.h>
#include "actualzip.h"
#include "tools.h"
#include "cosuplatform.h"

int create_actual_zip(char *zipfile, struct buffers *bufs)
{
    int ret = 0;

    zipFile zf = zipOpen(zipfile, access(zipfile, F_OK) == 0 ? APPEND_STATUS_ADDINZIP : 0);
    if (zf == NULL)
    {
        printerr("Failed creating a zip file");
        return 1;
    }

    time_t ti = time(NULL);
    if (ti == -1)
    {
        perror(NULL);
        ret = 3;
        goto zipc;
    }

    struct tm *lt = localtime(&ti);
    if (lt == NULL)
    {
        printerr("Failed getting time");
        ret = 4;
        goto zipc;
    }

    zip_fileinfo mapf;
    mapf.tmz_date.tm_sec  = lt->tm_sec;
    mapf.tmz_date.tm_min  = lt->tm_min;
    mapf.tmz_date.tm_hour = lt->tm_hour;
    mapf.tmz_date.tm_mday = lt->tm_mday;
    mapf.tmz_date.tm_mon  = lt->tm_mon;
    mapf.tmz_date.tm_year = lt->tm_year;

    mapf.dosDate = 0;
    mapf.internal_fa = 0;
    mapf.external_fa = 0;

    char *cvname;

    if (bufs->audname)
    {
        zip_fileinfo audf = mapf;
        char *cvname = convert_to_cp932(bufs->audname);

        if (cvname == NULL)
        {
            printerr("Failed to convert name");
            ret = 10;
            goto zipc;
        }

        if (!(zipOpenNewFileInZip(zf, cvname, &audf, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_NO_COMPRESSION) == ZIP_OK
                && zipWriteInFileInZip(zf, bufs->audbuf, bufs->audlast) == ZIP_OK
                && zipCloseFileInZip(zf) == ZIP_OK))
        {
            printerr("Error writing an audio file in zip");
            free(cvname);
            ret = 2;
            goto zipc;
        }

        free(cvname);
    }

    cvname = convert_to_cp932(bufs->mapname);

    if (cvname == NULL)
    {
        printerr("Failed to convert name");
        ret = 10;
        goto zipc;
    }

    if (!(zipOpenNewFileInZip(zf, cvname, &mapf, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_NO_COMPRESSION) == ZIP_OK
            && zipWriteInFileInZip(zf, bufs->mapbuf, bufs->maplast) == ZIP_OK
            && zipCloseFileInZip(zf) == ZIP_OK))
    {
        printerr("Error writing a map file in zip");
        free(cvname);
        ret = 2;
        goto zipc;
    }

    free(cvname);

zipc:
    if (zipClose(zf, NULL) != ZIP_OK)
    {
        printerr("Error closing a zip file");
        return 2;
    }

    return ret;
}
