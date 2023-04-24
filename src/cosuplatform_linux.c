#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "tools.h"
#include "cosuplatform.h"

int fork_launch(char* cmd)
{
   int ret = fork();
   if (ret == 0)
   {
      exit(system(cmd));
   }
   return ret;
}

char *read_file(const char *file, int *size)
{
   int fd = open(file, O_RDONLY);
   if (fd == -1)
   {
      perror(file);
      return NULL;
   }

   int cursize = 0;
   char *buf = NULL;
   ssize_t rb = 0;
   ssize_t curpoint = 0;
   do
   {
      char *rebuf = (char*) realloc(buf, (cursize += 1024));
      if (rebuf == NULL)
      {
         printerr("Failed allocating memory while reading a file");
         free(buf);
         close(fd);
         return NULL;
      }

      buf = rebuf;
      rb = read(fd, buf + curpoint, 1024 - 1);
      if (rb == -1)
      {
         printerr("Failed reading a file");
         free(buf);
         close(fd);
         return NULL;
      }

      curpoint += rb;
   }
   while (rb >= 1024 - 1);

   *(buf + curpoint) = '\0';
   if (size != NULL) *size = curpoint;
   close(fd);
   return buf;
}
