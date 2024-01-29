#pragma once
#include <stdbool.h>

struct songpath_status
{
    char *songf;

    char *save;
    char *path;

    bool within;
};

void songpath_init(struct songpath_status *st);
void songpath_free(struct songpath_status *st);
bool songpath_get(struct songpath_status *st, char **path);
