#pragma once
#include <stdbool.h>

// i realized there is SoundTouchDLL, after writing this lmao
typedef float SAMPLETYPE; // assume since this binding is used to link dynamically

#ifdef __cplusplus
extern "C"
{
#endif

struct SoundTouchState_;
typedef struct SoundTouchState_ *SoundTouchState;

SoundTouchState soundtouch_new();

void soundtouch_remove(SoundTouchState state);

const char *soundtouch_get_version_string();

unsigned int soundtouch_get_version_id();

void soundtouch_set_rate(SoundTouchState state, double new_rate);

void soundtouch_set_tempo(SoundTouchState state, double new_tempo);

void soundtouch_set_rate_change(SoundTouchState state, double new_rate);

void soundtouch_set_tempo_change(SoundTouchState state, double new_tempo);

void soundtouch_set_pitch(SoundTouchState state, double new_pitch);

void soundtouch_set_pitch_octaves(SoundTouchState state, double new_pitch);

void soundtouch_set_pitch_semitones_int(SoundTouchState state, int new_pitch);

void soundtouch_set_pitch_semitones_double(SoundTouchState state, double new_pitch);

void soundtouch_set_channels(SoundTouchState state, unsigned int num_channels);

void soundtouch_set_sample_rate(SoundTouchState state, unsigned int srate);

void soundtouch_flush(SoundTouchState state);

void soundtouch_put_samples(SoundTouchState state, SAMPLETYPE *samples, unsigned int num_samples);

unsigned int soundtouch_receive_samples(SoundTouchState state, SAMPLETYPE *output, unsigned int max_samples);

unsigned int soundtouch_remove_samples(SoundTouchState state, unsigned int max_samples);

void soundtouch_clear(SoundTouchState state);

int soundtouch_set_setting(SoundTouchState state, int setting_id, int value);

int soundtouch_get_setting(SoundTouchState state, int setting_id);

unsigned int soundtouch_num_unprocessed_samples(SoundTouchState state);

#ifdef __cplusplus
}
#endif
