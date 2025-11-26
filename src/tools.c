#include "tools.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef WIN32
#include <windows.h>
#define gtime() GetTickCount()
#define strncasecmp(x,y,z) _strnicmp(x,y,z)
#else
#include <time.h>
#define gtime() time(NULL)
#include <strings.h>
#endif

void randominit()
{
    srand(gtime());
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

// this is case insensitive since it's mostly used to compare extension
int endswith(const char *str, const char *suffix)
{
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;

    return strncasecmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

char *replace_string(const char *source, const char *match, const char *replace)
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

char *trim(char *str, size_t *res_size)
{
    size_t size = res_size == NULL ? strlen(str) : *res_size;
    char *start = str;
    char *end = start + size;
    while (isspace(*start) && *start != '\0')
    {
        start++;
        size--;
    }

    while (isspace(*(--end)) && end >= start)
    {
        size--;
    }
    *(end + 1) = '\0';

    if (res_size != NULL) *res_size = size;
    return start;
}
