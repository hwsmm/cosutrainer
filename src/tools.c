#include "tools.h"

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

int count_digits(int n)
{
   int count = 1;
   while (n > 9)
   {
      n /= 10;
      count++;
   }
   return count;
}
