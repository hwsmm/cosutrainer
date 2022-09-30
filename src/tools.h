#pragma once
#include <string.h>
#include <sys/types.h>

#define CMPSTR(x, y) (strncmp(x, y, sizeof((y)) - 1) == 0)
// compare the first part of string with a fixed string

#define CUTFIRST(x, y) (x + sizeof(y) - 1)

void remove_newline(char* line);
int ends_with(const char *str, const char *suffix);

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
