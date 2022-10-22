#include <SoundTouch.h>
#include <STTypes.h>
#include "soundtouch-c.h"

using namespace soundtouch;

struct SoundTouchState_
{
   SoundTouch *st;
};

SoundTouchState soundtouch_new()
{
   SoundTouchState state = new SoundTouchState_();
   state->st = new SoundTouch();
   return state;
}

void soundtouch_remove(SoundTouchState state)
{
   delete state->st;
   delete state;
}

const char *soundtouch_get_version_string()
{
   return SoundTouch::getVersionString();
}

unsigned int soundtouch_get_version_id()
{
   return SoundTouch::getVersionId();
}

void soundtouch_set_rate(SoundTouchState state, double new_rate)
{
   state->st->setRate(new_rate);
}

void soundtouch_set_tempo(SoundTouchState state, double new_tempo)
{
   state->st->setTempo(new_tempo);
}

void soundtouch_set_rate_change(SoundTouchState state, double new_rate)
{
   state->st->setRateChange(new_rate);
}

void soundtouch_set_tempo_change(SoundTouchState state, double new_tempo)
{
   state->st->setTempoChange(new_tempo);
}

void soundtouch_set_pitch(SoundTouchState state, double new_pitch)
{
   state->st->setPitch(new_pitch);
}

void soundtouch_set_pitch_octaves(SoundTouchState state, double new_pitch)
{
   state->st->setPitchOctaves(new_pitch);
}

void soundtouch_set_pitch_semitones_int(SoundTouchState state, int new_pitch)
{
   state->st->setPitchSemiTones(new_pitch);
}

void soundtouch_set_pitch_semitones_double(SoundTouchState state, double new_pitch)
{
   state->st->setPitchSemiTones(new_pitch);
}

void soundtouch_set_channels(SoundTouchState state, unsigned int num_channels)
{
   state->st->setChannels(num_channels);
}

void soundtouch_set_sample_rate(SoundTouchState state, unsigned int srate)
{
   state->st->setSampleRate(srate);
}

void soundtouch_flush(SoundTouchState state)
{
   state->st->flush();
}

void soundtouch_put_samples(SoundTouchState state, SAMPLETYPE *samples, unsigned int num_samples)
{
   state->st->putSamples(samples, num_samples);
}

unsigned int soundtouch_receive_samples(SoundTouchState state, SAMPLETYPE *output, unsigned int max_samples)
{
   return state->st->receiveSamples(output, max_samples);
}

unsigned int soundtouch_remove_samples(SoundTouchState state, unsigned int max_samples)
{
   return state->st->receiveSamples(max_samples);
}

void soundtouch_clear(SoundTouchState state)
{
   state->st->clear();
}

int soundtouch_set_setting(SoundTouchState state, int setting_id, int value)
{
   return state->st->setSetting(setting_id, value) ? 0 : 1;
}

int soundtouch_get_setting(SoundTouchState state, int setting_id)
{
   return state->st->getSetting(setting_id);
}

unsigned int soundtouch_num_unprocessed_samples(SoundTouchState state)
{
   return state->st->numUnprocessedSamples();
}
