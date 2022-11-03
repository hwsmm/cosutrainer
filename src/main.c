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
         printerr("Failed allocating buffer");
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
            printerr("Failed reallocating buffer");
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
         printerr("Failed allocating memory");
         free(buf);
         return 3;
      }
      snprintf(path, pathsize, "%s/%s", song_folder, buf);
      remove_newline(path);
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
   if (!actual_zip && bterr == 0)
   {
      int fd = open(bufs.mapname, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
      if (write(fd, bufs.mapbuf, bufs.maplast) == -1)
      {
         printerr("Error writing a map");
         bterr = 10;
      }

      if (bufs.audname)
      {
         int fdm = open(bufs.audname, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
         if (write(fdm, bufs.audbuf, bufs.audlast) == -1)
         {
            printerr("Error writing an audio file");
            bterr = 10;
         }
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
   if (ziperr == 0 && tempzippath != NULL)
   {
      char *open_cmd = getenv("OSZ_HANDLER");
      if (open_cmd != NULL)
      {
         const char replace[] = "{osz}";
         int replace_occur = 0;
         for (int i = 0; i < strlen(open_cmd) - sizeof replace + 2; i++)
         {
            if (memcmp(open_cmd + i, replace, sizeof replace - 1) == 0) replace_occur++;
         }

         int real_size = strlen(open_cmd) + (tempzipstrlen - sizeof replace) * replace_occur + 1;
         real_cmd = (char*) malloc(real_size);
         char *iteration = real_cmd;
         for (int i = 0; i < strlen(open_cmd) + 1; i++)
         {
            if (memcmp(open_cmd + i, replace, sizeof replace - 1) == 0)
            {
               memcpy(iteration++, tempzippath, tempzipstrlen - 1);
               iteration += tempzipstrlen - 2;
               i += sizeof replace - 2;
            }
            else
            {
               *(iteration++) = *(open_cmd + i);
            }
         }

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
