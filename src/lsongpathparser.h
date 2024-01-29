#pragma once
#include <stdbool.h>

#ifndef WIN32

#ifdef __cplusplus
extern "C"
{
#endif

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

#ifdef __cplusplus
}
#endif

#endif
