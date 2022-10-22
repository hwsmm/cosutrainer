#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <mpg123.h>
#include <lame/lame.h>
#include <sndfile.h>
#include "soundtouch-c.h"
#include "tools.h"

int change_mp3_speed(const char* source, const char* dest, double speed, bool pitch)
{
   FILE *result = fopen(dest, "w+");
   if (result == NULL) return 1;

   mpg123_handle *mh = NULL;
   struct mpg123_frameinfo fi;
   SoundTouchState state;
   lame_global_flags *gfp = NULL;

   float *buffer = NULL;
   size_t buffer_size = 0;
   float *convbuf = NULL;
   size_t convbuf_size = 0;
   unsigned char *mp3buf = NULL;
   size_t mp3buf_size = 0;

   long rate = 0;
   int encoding = 0;
   int channels = 0;

   int err = 0;
   int lamerr = 0;
   size_t werr = 0;

   int bufsizesample = 0;
   size_t samplecount = 0;
   size_t done = 0;
   size_t wanted = 0;
   unsigned int processedsamples = 0;
   unsigned int fulllength = 0;
   bool flush = false;

   int success = 1;

   err = mpg123_init();
   if (err != MPG123_OK || (mh = mpg123_new(NULL, &err)) == NULL)
   {
      goto cleanup;
   }

   mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT|MPG123_GAPLESS, 0.);

   if (mpg123_open(mh, source) != MPG123_OK || mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK)
   {
      goto cleanup;
   }

   mpg123_scan(mh);
   fulllength = mpg123_length(mh);

   mpg123_info(mh, &fi);
   mpg123_format_none(mh);
   mpg123_format(mh, rate, channels, encoding);
   buffer_size = mpg123_outblock(mh);
   buffer = (float*) malloc(buffer_size);
   convbuf = (float*) malloc((convbuf_size = buffer_size));
   if (buffer == NULL || convbuf == NULL) goto cleanup;

   state = soundtouch_new();
   soundtouch_set_sample_rate(state, rate);
   soundtouch_set_channels(state, channels);
   if (!pitch) soundtouch_set_tempo_change(state, (speed - 1.0) * 100.0);
   else soundtouch_set_rate_change(state, (speed - 1.0) * 100.0);

   gfp = lame_init();
   lame_set_num_channels(gfp, channels);
   lame_set_in_samplerate(gfp, rate);
   switch (fi.vbr)
   {
   case MPG123_VBR:
      lame_set_VBR(gfp, vbr_mtrh);
      lame_set_VBR_q(gfp, 2);
      lame_set_VBR_min_bitrate_kbps(gfp, 128);
      lame_set_VBR_max_bitrate_kbps(gfp, 192);
      break;
   case MPG123_ABR:
      lame_set_VBR(gfp, vbr_abr);
      lame_set_VBR_q(gfp, 2);
      lame_set_VBR_mean_bitrate_kbps(gfp, fi.abr_rate);
      break;
   default:
      lame_set_brate(gfp, fi.bitrate);
      break;
   }

   if (lame_init_params(gfp) < 0)
   {
      goto cleanup;
   }

   bufsizesample = convbuf_size / sizeof(float) / channels;
   bool seco = false;

   while (1)
   {
      if (err == MPG123_DONE) flush = true;

      if (!flush)
      {
         err = mpg123_read(mh, (unsigned char*) buffer, buffer_size, &done);
         processedsamples += (samplecount = (done/sizeof(float))/channels);
         if (seco) printf("\r%d%%", processedsamples * 100 / fulllength), seco = false;
         else seco = true;

         if (err == MPG123_OK || err == MPG123_DONE)
         {
            soundtouch_put_samples(state, buffer, samplecount);
         }
         else
         {
            goto cleanup;
         }
      }
      else
      {
         puts("\r100%");
         soundtouch_flush(state);
      }

      do
      {
         samplecount = soundtouch_receive_samples(state, convbuf, bufsizesample);
         if (samplecount == 0) continue;
         wanted = 1.25 * samplecount + 7200;
         if (mp3buf_size < wanted)
         {
            unsigned char* tmp = (unsigned char*) realloc(mp3buf, wanted);
            if (tmp == NULL) goto cleanup;

            mp3buf = tmp;
            mp3buf_size = wanted;
         }

         lamerr = lame_encode_buffer_interleaved_ieee_float(gfp, convbuf, samplecount, mp3buf, mp3buf_size);
         if (lamerr < 0)
         {
            goto cleanup;
         }

         werr = fwrite(mp3buf, 1, lamerr, result);
         if (werr != lamerr)
         {
            perror(dest);
            goto cleanup;
         }
      }
      while (samplecount != 0);

      if (flush) break;
   }


   lamerr = lame_encode_flush(gfp, mp3buf, mp3buf_size);
   if (lamerr > 0)
   {
      werr = fwrite(mp3buf, 1, lamerr, result);
      if (werr != lamerr)
      {
         perror(dest);
         goto cleanup;
      }
   }

   lame_mp3_tags_fid(gfp, result);

   success = 0;
cleanup:
   free(buffer);
   free(convbuf);
   free(mp3buf);

   if (result) fclose(result);
   if (state) soundtouch_remove(state);
   if (gfp) lame_close(gfp);
   if (mh)
   {
      mpg123_close(mh);
      mpg123_delete(mh);
      mpg123_exit();
   }
   return success;
}

int change_audio_speed_libsndfile(const char* source, const char* dest, double speed, bool pitch)
{
   int success = 1;
   SNDFILE *in, *out;
   SF_INFO info = { 0 };

   float buffer[1024] = { 0.0 };
   float convbuf[1024] = { 0.0 };
   int bufsamples = 0;

   sf_count_t readcount = 0;
   unsigned int convcount = 0;

   SoundTouchState state;
   bool flush = false;

   if ((in = sf_open(source, SFM_READ, &info)) == NULL || (out = sf_open(dest, SFM_WRITE, &info)) == NULL)
   {
      goto sndfclose;
   }

   bufsamples = 1024 / info.channels;

   state = soundtouch_new();
   soundtouch_set_sample_rate(state, info.samplerate);
   soundtouch_set_channels(state, info.channels);
   if (!pitch) soundtouch_set_tempo_change(state, (speed - 1.0) * 100.0);
   else soundtouch_set_rate_change(state, (speed - 1.0) * 100.0);

   while (!flush)
   {
      readcount = sf_read_float(in, buffer, 1024);

      if (readcount != 0) soundtouch_put_samples(state, buffer, readcount / info.channels);
      else
      {
         soundtouch_flush(state);
         flush = true;
      }

      do
      {
         convcount = soundtouch_receive_samples(state, convbuf, bufsamples);
         sf_writef_float(out, convbuf, convcount);
      }
      while (convcount != 0);
   }

   success = 0;
sndfclose:
   if (in) sf_close(in);
   if (out) sf_close(out);
   if (state) soundtouch_remove(state);
   return success;
}

int change_audio_speed(const char* source, const char* dest, double speed, bool pitch)
{
   if (endswith(source, ".mp3"))
   {
      return change_mp3_speed(source, dest, speed, pitch);
      // using mpg123/lame backend exclusively to make bug fixing easier
      // and there are some distros that has libsndfile without mp3 support since it's only released recently
      // libsndfile also uses mpg123/lame anyway
   }
   else return change_audio_speed_libsndfile(source, dest, speed, pitch);
}
