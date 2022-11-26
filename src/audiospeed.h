#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

int change_audio_speed(const char* source, struct buffers *bufs, double speed, bool pitch, float *progress);

#ifdef __cplusplus
}
#endif
