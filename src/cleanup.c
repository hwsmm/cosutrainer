#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include "tools.h"

int main(int argc, char *argv[])
{
   char *mainfd = NULL;
   if (argc >= 2)
   {
      mainfd = argv[1];
   }
   else
   {
      mainfd = getenv("OSU_SONG_FOLDER");
   }

   if (mainfd == NULL)
   {
      printerr("Not enough arguments.");
      return 1;
   }

   if (chdir(mainfd))
   {
      perror(mainfd);
      return 1;
   }

   DIR *songrtd;
   struct dirent *ent;
   if (!(songrtd = opendir(".")))
   {
      perror(argv[1]);
      return 1;
   }

   while ((ent = readdir(songrtd)))
   {
      if (*(ent->d_name) == '.')
      {
         continue;
      }

      if (chdir(ent->d_name))
      {
         perror(ent->d_name);
         continue;
      }
      // unlink("cosu");

      DIR *songd;
      struct dirent *sent;
      if (!(songd = opendir(".")))
      {
         perror(ent->d_name);
         return 1;
      }

      char mp3s[512][1024];
      int index = 0;

      while ((sent = readdir(songd)))
      {
         if (endswith(sent->d_name, ".osu"))
         {
            FILE *map = fopen(sent->d_name, "r");
            char line[1024] = {0};
            if (map)
            {
               while (fgets(line, sizeof line, map))
               {
                  if (CMPSTR(line, "AudioFilename"))
                  {
                     remove_newline(line);
                     char* filename = line + 15;
                     if (endswith(filename, ".mp3") || endswith(filename, ".ogg"))
                     {
                        if (++index > 512 || strlen(filename) + 1 > 1024)
                        {
                           printerr("i didn't know this could be insufficient. report this");
                           return 2;
                        }
                        strcpy(mp3s[index], filename);
                     }
                     break;
                  }
               }
               fclose(map);
            }
            else
            {
               perror(sent->d_name);
            }
         }
      }

      rewinddir(songd);
      int removed = 0;

      while ((sent = readdir(songd)))
      {
         if (endswith(sent->d_name, ".mp3") || endswith(sent->d_name, ".ogg"))
         {
            int i;
            bool found = false;
            for (i = 0; i < index + 1; i++)
            {
               if (strcmp(sent->d_name, mp3s[i]) == 0)
               {
                  found = true;
                  break;
               }
            }

            if (!found)
            {
               if (unlink(sent->d_name) == 0)
               {
                  removed++;
               }
               else
               {
                  perror(sent->d_name);
               }
            }
         }
      }

      if (closedir(songd))
      {
         perror(ent->d_name);
         return 1;
      }

      if (removed > 0)
      {
         printf("%s: removed %d mp3(s).\n", ent->d_name, removed);
      }

      if (chdir(".."))
      {
         perror(mainfd);
         continue;
      }
   }

   if (closedir(songrtd))
   {
      perror(mainfd);
      return 1;
   }
   return 0;
}