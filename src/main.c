#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include "mapspeed.h"
#include "emptyzip.h"

int main(int argc, char *argv[])
{
   if (argc < 3)
   {
      printf("Not enough arguments.\n");
      return 1;
   }

   char *path;
   if (strcmp(argv[1], "auto") == 0)
   {
      char buf[2048] = {0};
      char *song_folder = getenv("OSU_SONG_FOLDER");
      if (song_folder == NULL)
      {
         printf("Set OSU_SONG_FOLDER variable to use 'auto' option.");
         return 2;
      }

      strcpy(buf, song_folder);
      strcat(buf, "/");

      int buflen = strlen(buf);
      FILE *map_path = fopen("/tmp/osu_path", "r");
      if (map_path != NULL && fgets(buf + buflen, sizeof buf - buflen, map_path) != NULL)
      {
         path = buf;
      }
      else
      {
         perror("/tmp/osu_path");
         return -1;
      }
   }
   else
   {
      path = argv[1];
   }

   char *dir_delimiter = strrchr(path, '/');
   char *filename;
   if (dir_delimiter == NULL)
   {
      filename = path;
   }
   else
   {
      *dir_delimiter = '\0';
      filename = dir_delimiter + 1;
      if (chdir(path))
      {
         perror(path);
         return -1;
      }
   }

   char *identifier = "";
   double speed = strtof(argv[2], &identifier);
   bool bpm = identifier != NULL ? strncmp(identifier, "bpm", 3) == 0 : false;

   struct difficulty diff = { -2, -2, -1, -1 }; // -1 means scaling diffs to speed rate
   bool pitch = false;
   enum FLIP flip = none;

   if (argc >= 4)
   {
      int i;
      for (i = 3; i < argc; i++)
      {
         float *dest = NULL;
         char *curarg = argv[i];
         switch (*curarg)
         {
         case 'p':
            pitch = true;
            continue;
         case 'x':
            flip = xflip;
            continue;
         case 'y':
            flip = yflip;
            continue;
         case 't':
            flip = transpose;
            continue;
         case 'h':
            dest = &(diff.hp);
            break;
         case 'c':
            dest = &(diff.cs);
            break;
         case 'o':
            dest = &(diff.od);
            break;
         case 'a':
            dest = &(diff.ar);
            break;
         default:
            continue;
         }

         if (dest != NULL)
         {
            if (*(curarg + 1) == 'f') *dest = -2;
            else *dest = atof(curarg + 1);
         }
      }
   }

   int bterr = edit_beatmap(filename, speed, bpm, diff, pitch, flip);
   int ziperr = 0;
   if (bterr == 0 && dir_delimiter != NULL)
   {
      char *songfdname = strrchr(path, '/') + 1;

      char tempzippath[1024];
      snprintf(tempzippath, sizeof tempzippath, "../%s.osz", songfdname);
      ziperr = create_empty_zip(tempzippath);
   }
   return (ziperr != 0 || bterr != 0) ? 1 : 0;
}
