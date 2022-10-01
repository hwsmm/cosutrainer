#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <pwd.h>

#include "sigscan.h"
#include "tools.h"

pid_t osu = -1;

int find_and_set_osu()
{
   if (osu > 0 && kill(osu, 0) == 0)
   {
      return 1;
   }

   pid_t osupid;
   char pidbuf[16] = {0};
   int ref = -3;

   pid_t candidate = 0;
   DIR *d;
   struct dirent *ent;
   int commfd;

   if (chdir("/proc"))
   {
      perror("/proc");
      ref = -1;
   }
   else
   {
      d = opendir(".");
      if (!d)
      {
         perror("/proc");
         ref = -1;
      }
      else
      {
         while ((ent = readdir(d)))
         {
            candidate = atoi(ent->d_name);
            if (candidate > 0)
            {
               if (!chdir(ent->d_name))
               {
                  commfd = open("comm", O_RDONLY);
                  ssize_t readbytes = read(commfd, &pidbuf, 16);
                  if (close(commfd) < 0)
                  {
                     printf("Error while closing file\n");
                  }

                  if (readbytes != -1)
                  {
                     if (CMPSTR(pidbuf, "osu!.exe"))
                     {
                        osupid = candidate;
                        ref = 0;
                        break;
                     }
                  }
                  if (chdir(".."))
                  {
                     perror("..");
                     ref = -1;
                     break;
                  }
               }
            }
         }
         if (closedir(d))
         {
            perror("/proc");
         }
      }
   }

   if (chdir("/tmp"))
   {
      perror("/tmp");
      ref = -1;
   }

   if (ref < 0)
   {
      return ref;
   }

   if (osupid != osu)
   {
      osu = osupid;
      return 2;
   }
   return 1;
}

bool readmemory(void *address, void *buffer, size_t len)
{
   struct iovec local[1] = {};
   struct iovec remote[1] = {};

   local[0].iov_len = len;
   local[0].iov_base = (void *)buffer;

   remote[0].iov_base = address;
   remote[0].iov_len = local[0].iov_len;

   ssize_t nread = process_vm_readv(osu, local, 2, remote, 1, 0);

   if (nread != local[0].iov_len)
   {
      perror(NULL);
      return false;
   }

   return true;
}

void* match_pattern()
{
   // "F8 01 74 04 83 65"
   const char basepattern[] = { 0xf8, 0x01, 0x74, 0x04, 0x83, 0x65 };
   return find_pattern(basepattern, 6);
}

void* find_pattern(const char bytearray[], const int pattern_size)
{
   char mapsfile[32];
   FILE *maps;
   char line[1024];
   void *result = NULL;

   snprintf(mapsfile, 32, "/proc/%d/maps", osu);

   maps = fopen(mapsfile, "r");
   if (!maps)
   {
      perror(mapsfile);
      return NULL;
   }

   bool found = false;

   while (fgets(line, sizeof line, maps))
   {
      if (found) break;

      char *startstr = strtok(line, "-");
      char *endstr = strtok(NULL, " ");
      char *permstr = strtok(NULL, " ");

      if (*permstr != 'r')
      {
         continue;
      }

      void *startptr = (void*) strtol(startstr, NULL, 16);
      void *endptr = (void*) strtol(endstr, NULL, 16);

      long size = endptr - startptr;

      char *buffer = (char*) malloc(size);
      if (buffer == NULL)
      {
         printf("Failed allocating memory\n");
         break;
      }

      if (!readmemory(startptr, buffer, size))
      {
         free(buffer);
         continue;
      }

      long i;
      for (i = 0; i < size - pattern_size; i++)
      {
         if (memcmp(buffer + i, bytearray, pattern_size) == 0)
         {
            result = startptr + i;
            found = true;
            break;
         }
      }
      free(buffer);
   }

   if (fclose(maps) == EOF)
   {
      perror(mapsfile);
   }

   return result;
}

void* get_beatmap_ptr(void* base_address)
{
   void *beatmap_2ptr = NULL;
   void *beatmap_ptr = NULL;

   if (!readmemory(base_address + BEATMAP_OFFSET, &beatmap_2ptr, 4))
      return NULL;

   if (!readmemory(beatmap_2ptr, &beatmap_ptr, 4))
      return NULL;

   return beatmap_ptr;
}

int get_mapid(void *base_address)
{
   int id = 0;
   if (!readmemory(get_beatmap_ptr(base_address) + MAPID_OFFSET, &id, 4))
      return -1;

   return id;
}

char *get_mappath(void *base_address)
{
   void *beatmap_ptr = get_beatmap_ptr(base_address);
   if (beatmap_ptr == NULL) return NULL;

   void *folder_ptr = NULL;
   void *path_ptr = NULL;
   int foldersize = 0;
   int pathsize = 0;

   if (!readmemory(beatmap_ptr + FOLDER_OFFSET, &folder_ptr, 4))
      return NULL;

   if (!readmemory(folder_ptr + 4, &foldersize, 4))
      return NULL;

   if (!readmemory(beatmap_ptr + PATH_OFFSET, &path_ptr, 4))
      return NULL;

   if (!readmemory(path_ptr + 4, &pathsize, 4))
      return NULL;

   int size = foldersize + 1 + pathsize + 1; // null and /
   uint16_t *buf = (uint16_t*) calloc(size, 2);
   char *songpath = (char*) calloc(size, 1);

   if (!buf || !songpath)
      goto readfail;

   if (!readmemory(folder_ptr + 8, buf, foldersize * 2))
      goto readfail;

   buf[foldersize] = '/';

   if (!readmemory(path_ptr + 8, buf + foldersize + 1, pathsize * 2))
      goto readfail;

   int i;
   for (i = 0; i < size; i++)
   {
      if (*(buf+i) > 127) goto readfail;
      *(songpath+i) = *(buf+i);
   }

   free(buf);
   return songpath;

readfail:
   free(songpath);
   free(buf);
   return NULL;
} // may change it to wchar_t since path may contain unicode characters, but i will just use char here for now to keep things simple

volatile int run = 1;
void gotquitsig(int sig)
{
   run = 0;
}

int main(int argc, char *argv[])
{
   setbuf(stdout, NULL);
   int find_osu = -1;
   void *base = NULL;

   char *songpath = NULL;
   char *oldpath = NULL;

   struct sigaction act;
   act.sa_handler = gotquitsig;
   if (sigaction(SIGINT, &act, NULL) != 0 && sigaction(SIGTERM, &act, NULL) != 0)
   {
      perror(NULL);
      return -3;
   }

   while (run)
   {
      if (find_osu < 0)
      {
         find_osu = find_and_set_osu();
         if (find_osu < 0)
         {
            putchar('.');
            goto contin;
         }
         else
         {
            putchar('\n');
         }
      }
      else
      {
         find_osu = find_and_set_osu();
      }

      if (find_osu >= 0)
      {
         if (find_osu == 2)
         {
            printf("osu! is found. ");
         }

         if (base == NULL)
         {
            printf("starting to scan memory...\n");
            base = match_pattern();
            if (base != NULL)
            {
               printf("scan succeeded. you can now use 'auto' option\n");
            }
            else
            {
               printf("failed scanning memory: it could be because it's too early\n");
               printf("will retry in 3 seconds...\n");
               sleep(3);
               continue;
            }
         }

         songpath = get_mappath(base);
         if (songpath != NULL)
         {
            if (oldpath != NULL && strcmp(songpath, oldpath) == 0)
            {
               free(songpath);
               goto contin;
            }
            else
            {
               free(oldpath);
               oldpath = songpath;
            }
         }
         else
         {
            printf("failed reading memory: scanning again...\n");
            base = NULL;
            goto contin;
         }

         int fd = open("/tmp/osu_path", O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
         if (fd == -1)
         {
            perror("/tmp/osu_path");
         }
         else
         {
            ssize_t w = write(fd, songpath, strlen(songpath));
            if (w == -1)
            {
               perror("/tmp/osu_path");
            }
            close(fd);
         }
      }
      else
      {
         printf("process lost! waiting for osu");
      }
contin:
      sleep(1);
   }
   if (unlink("/tmp/osu_path") != 0)
   {
      perror("/tmp/osu_path");
   }
   free(oldpath);
   return 0;
}
