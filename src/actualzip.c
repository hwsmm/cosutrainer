#include <zip.h>
#include <time.h>
#include "actualzip.h"
#include "tools.h"

int create_actual_zip(char *zipfile, struct buffers *bufs)
{
   zipFile zf = zipOpen(zipfile, APPEND_STATUS_ADDINZIP);
   if (zf == NULL)
   {
      printerr("Failed creating a zip file");
      return 1;
   }

   zip_fileinfo audf;
   zip_fileinfo mapf;

   audf.tmz_date.tm_sec = audf.tmz_date.tm_min = audf.tmz_date.tm_hour =
                             audf.tmz_date.tm_mday = audf.tmz_date.tm_mon = audf.tmz_date.tm_year = 0;
   audf.dosDate = 0;
   audf.internal_fa = 0;
   audf.external_fa = 0;

   mapf.tmz_date.tm_sec = mapf.tmz_date.tm_min = mapf.tmz_date.tm_hour =
                             mapf.tmz_date.tm_mday = mapf.tmz_date.tm_mon = mapf.tmz_date.tm_year = 0;
   mapf.dosDate = 0;
   mapf.internal_fa = 0;
   mapf.external_fa = 0;

   time_t ti = time(NULL);
   if (ti == -1)
   {
      perror(NULL);
      return 3;
   }
   struct tm *lt = localtime(&ti);
   audf.tmz_date.tm_sec  = mapf.tmz_date.tm_sec  = lt->tm_sec;
   audf.tmz_date.tm_min  = mapf.tmz_date.tm_min  = lt->tm_min;
   audf.tmz_date.tm_hour = mapf.tmz_date.tm_hour = lt->tm_hour;
   audf.tmz_date.tm_mday = mapf.tmz_date.tm_mday = lt->tm_mday;
   audf.tmz_date.tm_mon  = mapf.tmz_date.tm_mon  = lt->tm_mon;
   audf.tmz_date.tm_year = mapf.tmz_date.tm_year = lt->tm_year;

   if (bufs->audname)
   {
      if (!(zipOpenNewFileInZip(zf, bufs->audname, &audf, NULL, 0, NULL, 0, NULL, 0, 0) == ZIP_OK
            && zipWriteInFileInZip(zf, bufs->audbuf, bufs->audlast) == ZIP_OK
            && zipCloseFileInZip(zf) == ZIP_OK))
      {
         printerr("Error writing an audio file in zip");
         return 2;
      }
   }

   if (!(zipOpenNewFileInZip(zf, bufs->mapname, &mapf, NULL, 0, NULL, 0, NULL, 0, 0) == ZIP_OK
         && zipWriteInFileInZip(zf, bufs->mapbuf, bufs->maplast) == ZIP_OK
         && zipCloseFileInZip(zf) == ZIP_OK))
   {
      printerr("Error writing a map file in zip");
      return 2;
   }

   if (zipClose(zf, NULL) != ZIP_OK)
   {
      printerr("Error closing a zip file");
      return 2;
   }
   return 0;
}
