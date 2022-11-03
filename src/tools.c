#include "tools.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

void randominit()
{
   srand(time(NULL));
}

// generate random string with alphabets and save it to 'string'
void randomstr(char *string, int size)
{
   int i;
   for (i = 0; i < size; i++)
   {
      int randomnum = rand() % 26;
      randomnum += 97;
      *(string + i) = randomnum;
   }
}

void remove_newline(char* line)
{
   char *newline = strchr(line, '\n');
   if (newline != NULL)
   {
      if (newline != line && *(newline - 1) == '\r') *(newline - 1) = '\0';
      else *newline = '\0';
   }
}

int endswith(const char *str, const char *suffix)
{
   size_t lenstr = strlen(str);
   size_t lensuffix = strlen(suffix);
   if (lensuffix > lenstr)
      return 0;

   return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int count_digits(unsigned long n)
{
   int count = 1;
   while (n > 9)
   {
      n /= 10;
      count++;
   }
   return count;
}

char *replace_string(char *source, char *match, char *replace)
{
   char *result = NULL;
   int i, s, e;
   int srclen = strlen(source);
   int matlen = strlen(match);
   int replen = strlen(replace);

   int real_size = srclen + 1;
   result = (char*) malloc(real_size);
   if (!result)
   {
      printerr("Failed allocating memory while replacing string");
      return NULL;
   }

   for (i = 0, s = 0; i < real_size; i++, s++)
   {
      if (memcmp(source + s, match, matlen) == 0)
      {
         char* tempa = (char*) realloc(result, (real_size += replen - matlen));
         if (!tempa)
         {
            printerr("Failed reallocating memory");
            free(result);
            return NULL;
         }
         result = tempa;

         for (e = 0; e < replen; e++, i++)
         {
            *(result + i) = *(replace + e);
         }
         s += matlen - 1;
         i--;
      }
      else
      {
         *(result + i) = *(source + s);
      }
   }

   return result;
}

char *read_file(char *file, int *size)
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
