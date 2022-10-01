#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>

#include "mapspeed.h"
#include "emptyzip.h"

int main(int argc, char *argv[])
{
   if (argc < 3)
   {
      printf("Not enough arguments.\n");
      return 1;
   }

   char *path = NULL;
   bool osumem = (strcmp(argv[1], "auto") == 0);
   if (osumem)
   {
      char *song_folder = getenv("OSU_SONG_FOLDER");
      if (song_folder == NULL)
      {
         printf("Set OSU_SONG_FOLDER variable to use 'auto' option.");
         return 2;
      }

      int path_fd = open("/tmp/osu_path", O_RDONLY);
      if (path_fd == -1)
      {
         perror("/tmp/osu_path");
         return 2;
      }

      int cursize = 2048;
      char *buf = (char*) malloc(cursize);
      if (buf == NULL)
      {
         printf("Failed allocating buffer\n");
         free(buf);
         return 3;
      }
      char *tmpbuf = NULL;

      ssize_t readbytes = read(path_fd, buf, cursize - 1);
      ssize_t rb = 0;
      do
      {
         if (readbytes == -1 || rb == -1)
         {
            perror("/tmp/osu_path");
            close(path_fd);
            free(buf);
            return 2;
         }
         if (!(readbytes >= cursize - 1)) break;

         tmpbuf = (char*) realloc(buf, cursize + 1024);
         if (tmpbuf == NULL)
         {
            printf("Failed reallocating buffer\n");
            close(path_fd);
            free(buf);
            return 3;
         }
         cursize += 1024;
         buf = tmpbuf;
         rb = read(path_fd, buf + readbytes, 1024 - 1);
         readbytes += rb;
      }
      while (1);

      close(path_fd);
      *(buf + readbytes) = '\0';

      int pathsize = strlen(song_folder) + 1 + readbytes + 1;
      path = (char*) malloc(pathsize);
      if (path == NULL)
      {
         printf("Failed allocating\n");
         free(buf);
         return 3;
      }
      snprintf(path, pathsize, "%s/%s", song_folder, buf);

      free(buf);
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

   char *identifier = NULL;
   double speed = strtof(argv[2], &identifier);
   bool bpm = identifier != NULL && strncmp(identifier, "bpm", 3) == 0;

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

      int tempzipstrlen = 3 + strlen(songfdname) + 4 + 1;
      char *tempzippath = (char*) malloc(tempzipstrlen);
      if (tempzippath != NULL)
      {
         snprintf(tempzippath, tempzipstrlen, "../%s.osz", songfdname);
         ziperr = create_empty_zip(tempzippath);
         free(tempzippath);
      }
      else
      {
         ziperr = 3;
      }
   }

   if (osumem) free(path);

   return (ziperr != 0 || bterr != 0) ? 1 : 0;
}
