#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>

#include "buffers.h"
#include "mapspeed.h"
#include "actualzip.h"
#include "emptyzip.h"
#include "tools.h"

int main(int argc, char *argv[])
{
   if (argc < 3)
   {
      printerr("Not enough arguments.");
      return 1;
   }

   char *path = NULL;
   bool osumem = (strcmp(argv[1], "auto") == 0);
   if (osumem)
   {
      char *song_folder = getenv("OSU_SONG_FOLDER");
      if (song_folder == NULL)
      {
         printerr("Set OSU_SONG_FOLDER variable to use 'auto' option.");
         return 2;
      }

      int readbytes = 0;
      char *song_path = read_file("/tmp/osu_path", &readbytes);
      if (song_path == NULL)
      {
         return 2;
      }

      int pathsize = strlen(song_folder) + 1 + readbytes + 1;
      path = (char*) malloc(pathsize);
      if (path == NULL)
      {
         printerr("Failed allocating memory");
         free(song_path);
         return 3;
      }
      snprintf(path, pathsize, "%s/%s", song_folder, song_path);
      remove_newline(path);
      free(song_path);
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
   enum SPEED_MODE rate_mode = guess;
   if (identifier != NULL)
   {
      if (*identifier == 'x')
      {
         rate_mode = rate;
      }
      else if (CMPSTR(identifier, "bpm"))
      {
         rate_mode = bpm;
      }
   }

   struct difficulty diff = { { fix, 0, 0, 0 }, { fix, 0, 0, 0 }, { scale, 0, 0, 0 }, { scale, 0, 0, 0 } };
   bool pitch = false;
   enum FLIP flip = none;

   if (argc >= 4)
   {
      int i;
      for (i = 3; i < argc; i++)
      {
         struct diff *dest = NULL;
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
            if (*(curarg + 1) == 'f') dest->mode = fix;
            else
            {
               char *iden = NULL;
               dest->user_value = strtof(curarg + 1, &iden);
               if (iden != NULL && *iden == 'c') dest->mode = cap;
               else dest->mode = specify;
            }
         }
      }
   }

   struct buffers bufs;
   int bterr = 1;
   if (buffers_init(&bufs) == 0)
   {
      bterr = edit_beatmap(filename, speed, rate_mode, diff, pitch, flip, &bufs);
   }

   bool actual_zip = (getenv("COSU_EMPTY_ZIP") == NULL);
   if (!(actual_zip && osumem) && bterr == 0)
   {
      int fd = open(bufs.mapname, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
      if (write(fd, bufs.mapbuf, bufs.maplast) == -1)
      {
         printerr("Error writing a map");
         bterr = 10;
      }
      close(fd);

      if (bufs.audname)
      {
         int fdm = open(bufs.audname, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
         if (write(fdm, bufs.audbuf, bufs.audlast) == -1)
         {
            printerr("Error writing an audio file");
            bterr = 10;
         }
         close(fdm);
      }
   }

   int ziperr = 0;
   char *tempzippath = NULL;
   int tempzipstrlen = 0;
   if (osumem && bterr == 0 && dir_delimiter != NULL)
   {
      char *songfdname = strrchr(path, '/');
      if (songfdname != NULL)
      {
         songfdname++;
         tempzipstrlen = 3 + strlen(songfdname) + 4 + 1;
         tempzippath = (char*) malloc(tempzipstrlen);
         if (tempzippath != NULL)
         {
            snprintf(tempzippath, tempzipstrlen, "../%s.osz", songfdname);
            ziperr = actual_zip ? create_actual_zip(tempzippath, &bufs) : create_empty_zip(tempzippath);
         }
         else
         {
            printerr("Failed allocating memory");
            ziperr = 3;
         }
      }
   }

   char *real_cmd = NULL;
   char *real_path = NULL;
   if (ziperr == 0 && tempzippath != NULL)
   {
      char *open_cmd = getenv("OSZ_HANDLER");
      if (open_cmd != NULL)
      {
         real_path = realpath(tempzippath, NULL);
         real_cmd = replace_string(open_cmd, "{osz}", real_path);

         int forkret = fork();
         if (forkret == 0)
         {
            ziperr = system(real_cmd);
         }
         else if (forkret == -1)
         {
            perror("fork");
         }
      }
   }

   if (osumem) free(path);
   buffers_free(&bufs);
   free(tempzippath);
   free(real_cmd);
   free(real_path);

   return ziperr != 0 ? ziperr : bterr != 0 ? 1 : 0;
}
