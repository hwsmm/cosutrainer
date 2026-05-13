#pragma once
#include "buffers.h"

#ifdef __cplusplus
extern "C"
{
#endif

int create_actual_zip(char *zipfile, struct buffers *bufs);
int check_zip_file(char* zipfile, char *file);

#ifdef __cplusplus
}
#endif
