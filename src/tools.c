#include "tools.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

#ifdef WIN32
#include <windows.h>
#define gtime() GetTickCount()
#else
#include <time.h>
#define gtime() time(NULL)
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

int endswith(const char *str, const char *suffix)
{
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;

    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
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

char *trim(char *str, int *res_size)
{
    int size = res_size == NULL ? strlen(str) : *res_size;
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

#ifndef WIN32
#include <unistd.h>
#include <dirent.h>
int try_convertwinpath(char *path, int pos)
{
    if (access(path, F_OK) == 0)
    {
        return 1;
    }

    bool last = false;
    char *start = path + pos;
    do
    {
        char *end = strchr(start, '/');
        last = end == NULL;
        char *tempend = last ? path + strlen(path) - 1 : end - 1;

        if (!last)
        {
            if (path + strlen(path) - 1 == end) // if / is at the end
            {
                last = true;
                *end = '\0';
                end--;
            }
        }

        bool trim = false;
        while (tempend > start)
        {
            // Windows path shouldn't end with period or space
            if (isspace(*tempend) || *tempend == '.')
            {
                if (!trim) trim = true;
                tempend--;
            }
            else
            {
                break;
            }
        }

        if (trim)
        {
            if (!last)
            {
                for (int i = 0; i < strlen(end) + 1; i++)
                {
                    char newchr = *(end + i);
                    *(tempend + 1 + i) = newchr;
                }
            }
            else
            {
                *(tempend + 1) = '\0';
            }
        }

        bool match = false;
        if (access(path, F_OK) == 0)
        {
            return 0;
        }
        else // try case insensitive searching through directory
        {
            char backup = *(start - 1);
            *(start - 1) = '\0';

            DIR *d = opendir(path);
            struct dirent *ent = NULL;
            if (d == NULL)
            {
                perror(path);
                *(start - 1) = backup;
                return -1;
            }
            *(start - 1) = backup;

            char *deli = trim ? strchr(start, '/') : end;
            if (deli != NULL) *deli = '\0';

            while ((ent = readdir(d)))
            {
                if (strcasecmp(ent->d_name, start) == 0) // may cause a problem with unicode string
                {
                    memcpy(start, ent->d_name, strlen(start));
                    match = true;
                    start = deli + 1;
                    break;
                }
            }

            if (deli != NULL) *deli = '/';
            closedir(d);

            if (!match)
                return -1;
            else if (last)
                return 0;
        }

        if (last)
            return -1;
    }
    while (1);
    return 0;
}
#else
// stub, since winapi functions already handle them well enough
int try_convertwinpath(char *path, int pos)
{
    if (access(path, F_OK) == 0)
    {
        return 1;
    }
    return -1;
}
#endif
