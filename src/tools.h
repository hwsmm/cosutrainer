#pragma once
#include <string.h>
#include <sys/types.h>

#define CMPSTR(x, y) (strncmp(x, y, sizeof((y)) - 1) == 0)
// compare the first part of string with a fixed string

#define CUTFIRST(x, y) (x + sizeof(y) - 1)

void remove_newline(char* line);
int endswith(const char *str, const char *suffix);
int count_digits(int n);
