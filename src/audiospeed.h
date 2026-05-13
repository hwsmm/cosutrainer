#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct audiospeed_config
{
    const char *source;
    double speed;
    bool pitch;
    double emuldt;
};

typedef void (*update_progress_cb)(void *data, float progress);

int change_audio_speed(struct audiospeed_config cfg, struct buffers *bufs, void *data, update_progress_cb callback);

#ifdef __cplusplus
}
#endif
