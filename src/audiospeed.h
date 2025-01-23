#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*update_progress_cb)(void *data, float progress);

int change_audio_speed(const char* source, struct buffers *bufs, double speed, bool pitch, void *data, update_progress_cb callback);

#ifdef __cplusplus
}
#endif
