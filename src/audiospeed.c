#include "audiospeed.h"
#include <stdlib.h>
#include <stdio.h>

int change_audio_speed(const char* source, const char* dest, double speed, bool pitch)
{
   int ret = 0;
   char *cmdline = (char*) malloc(2048);
   char *tempcmdline = NULL;
   if (cmdline == NULL) return 2;
   int cursize = 2048;
   while (1)
   {
      if (snprintf(cmdline, cursize, "ffmpeg -hide_banner -loglevel error -i \"%s\" -filter:a rubberband=tempo=%lf:pitch=%lf:smoothing=off \"%s\" -y",
                   source, speed, pitch ? speed : 1, dest) < cursize - 1) break;

      tempcmdline = (char*) realloc(cmdline, cursize + 1024);
      if (tempcmdline == NULL)
      {
         free(cmdline);
         return 2;
      }
      cmdline = tempcmdline;
      cursize += 1024;
   }

   if ((ret = system(cmdline)) != 0)
   {
      perror("ffmpeg");
   }
   free(cmdline);
   return ret;
}
