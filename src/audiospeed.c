#include "audiospeed.h"
#include "tools.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FFMPEG_CMD "ffmpeg -hide_banner -loglevel error -i \"%s\" -filter:a %s \"%s\" -y"

int change_audio_speed(const char* source, const char* dest, double speed, bool pitch)
{
   int ret = 0;
   char *cmdline = NULL;

   if (!pitch)
   {
      int count100 = 0;
      int count05 = 0;
      while (speed > 100)
      {
         count100++;
         speed /= 100;
      }
      while (speed < 0.5)
      {
         count05++;
         speed /= 0.5;
      }

      int size = (sizeof(FFMPEG_CMD) - 1) - 2 /*%s*/ + strlen(source) - 2 /*%s*/ + ((sizeof("atempo=100,") - 1) * count100) + ((sizeof("atempo=0.5,") - 1) * count05)
                 + (sizeof("atempo=") - 1) + count_digits(speed) + 1 /*.*/ + 12 - 2 + strlen(dest) + 1;

      cmdline = (char*) malloc(size);
      snprintf(cmdline, size, "ffmpeg -hide_banner -loglevel error -i \"%s\" -filter:a ", source);

      int i;
      for (i = 0; i < count100; i++)
      {
         strcat(cmdline, "atempo=100,");
      }
      for (i = 0; i < count05; i++)
      {
         strcat(cmdline, "atempo=0.5,");
      }

      char *sofar = strchr(cmdline, '\0');

      snprintf(sofar, size - (sofar - cmdline), "atempo=%.12lf \"%s\" -y", speed, dest);
   }
   else
   {
      int size = (sizeof(FFMPEG_CMD) - 1) - 2 + strlen(source) - 2 + ((sizeof("rubberband=tempo=%lf:pitch=%lf:smoothing=off")) - 3 - 3 - 1)
                 + ((count_digits(speed) + 1 + 12) * 2) + strlen(dest) + 1;
      cmdline = (char*) malloc(size);
      snprintf(cmdline, size, "ffmpeg -hide_banner -loglevel error -i \"%s\" -filter:a rubberband=tempo=%.12lf:pitch=%.12lf:smoothing=off \"%s\" -y", source, speed, speed, dest);
   }

   if ((ret = system(cmdline)) != 0)
   {
      perror("ffmpeg");
   }
   free(cmdline);
   return ret;
}
