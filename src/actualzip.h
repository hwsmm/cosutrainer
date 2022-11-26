#pragma once
#include "buffers.h"

#ifdef __cplusplus
extern "C"
{
#endif

int create_actual_zip(char *zipfile, struct buffers *bufs);

#ifdef __cplusplus
}
#endif
