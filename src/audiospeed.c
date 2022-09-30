#include "audiospeed.h"
#include <stdlib.h>
#include <stdio.h>

int change_audio_speed(const char* source, const char* dest, double speed, bool pitch)
{
   char cmdline[2048] = {0};
   snprintf(cmdline, sizeof cmdline, "ffmpeg -hide_banner -loglevel error -i \"%s\" -filter:a rubberband=tempo=%lf:pitch=%lf:smoothing=off \"%s\" -y", source, speed, pitch ? speed : 1, dest);

   if (system(cmdline) != 0)
   {
      perror("ffmpeg");
      return 1;
   }
   return 0;
}
