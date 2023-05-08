#pragma once
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>

#define CMPSTR(x, y) (strncmp(x, y, sizeof((y)) - 1) == 0)
// compare the first part of string with a fixed string

#define CUTFIRST(x, y) (x + sizeof(y) - 1)

#define CLAMP(x, min, max) x = x > max ? max : x < min ? min : x

#define printerr(s) fputs(s "\n", stderr)

#ifdef __cplusplus
extern "C"
{
#endif

void randominit();
void randomstr(char *string, int size);
void remove_newline(char* line);
int endswith(const char *str, const char *suffix);
int count_digits(unsigned long n);
char *replace_string(const char *source, const char *match, const char *replace);

#ifdef __cplusplus
}
#endif
