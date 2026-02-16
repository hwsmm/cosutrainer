#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sndfile.h>
#include <math.h>
#include <rubberband/RubberBandStretcher.h>
#include <stdexcept>
#include "tools.h"
#include "buffers.h"
#include "audiospeed.h"
#include "cosuplatform.h"
#include "cosuwindow.h"

using namespace RubberBand;
using namespace std;

sf_count_t sfvio_get_filelen(void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    return bufs->audlast;
}

sf_count_t sfvio_seek(sf_count_t offset, int whence, void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    return buffers_aud_seek(bufs, offset, whence);
}

sf_count_t sfvio_read(void *ptr, sf_count_t count, void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    return buffers_aud_get(bufs, ptr, count);
}

sf_count_t sfvio_write(const void *ptr, sf_count_t count, void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    if (buffers_aud_put(bufs, ptr, count) == 0) return count;
    else return 0;
}

sf_count_t sfvio_tell(void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    return bufs->audcur;
}

static void deinterlave_stereo(float *src, float *left, float *right, int frames)
{
    for (int i = 0; i < frames; i++)
    {
        left[i] = src[i * 2];
        right[i] = src[i * 2 + 1];
    }
}

static void interleave_stereo(float *dst, float *left, float *right, int frames)
{
    for (int i = 0; i < frames; i++)
    {
        dst[i * 2] = left[i];
        dst[i * 2 + 1] = right[i];
    }
}

int change_audio_speed_libsndfile(const char* source, struct buffers *bufs, double speed, bool pitch, double emuldt, void *data, update_progress_cb callback)
{
    int success = 1;
    SNDFILE *in = NULL, *out = NULL;
    SF_INFO info = { 0, 0, 0, 0, 0, 0 };
    SF_VIRTUAL_IO sfvio = { &sfvio_get_filelen, &sfvio_seek, &sfvio_read, &sfvio_write, &sfvio_tell };

    float buffer[256] = { 0.0 };
    float convbuf[256] = { 0.0 };
    float leftbuf[128] = { 0.0 };
    float rightbuf[128] = { 0.0 };
    float *rbbuf[2] = { leftbuf, rightbuf };
    int bufsamples = 0;

    bool flush = false;

    sf_count_t processed = 0;
    sf_count_t frames = 0;

    try
    {
        if ((in = sf_open(source, SFM_READ, &info)) == NULL)
        {
            throw -99;
        }

        frames = info.frames;
        info.frames = 0;
        bufsamples = sizeof(buffer) / sizeof(float) / info.channels;

        RubberBandStretcher rb(info.samplerate, info.channels,
        RubberBandStretcher::OptionProcessOffline | RubberBandStretcher::OptionEngineFiner | RubberBandStretcher::OptionChannelsTogether | (pitch ? RubberBandStretcher::OptionPitchHighQuality : 0), 1.0 / speed, pitch ? emuldt != 0 ? emuldt : speed : 1.0);
        rb.setMaxProcessSize(bufsamples);

        while (1)
        {
            sf_count_t readcount = sf_readf_float(in, buffer, bufsamples);
            deinterlave_stereo(buffer, leftbuf, rightbuf, readcount);
            rb.study(rbbuf, readcount, readcount < bufsamples);

            if (readcount < bufsamples)
                break;
        }

        sf_seek(in, 0, SEEK_SET);

        if ((out = sf_open_virtual(&sfvio, SFM_WRITE, &info, bufs)) == NULL)
        {
            throw -99;
        }

        int brm = sf_command(in, SFC_GET_BITRATE_MODE, NULL, 0);
        if (brm != -1)
            sf_command(out, SFC_SET_BITRATE_MODE, &brm, sizeof(int));

        while (!flush)
        {
            unsigned int convcount = 0;
            sf_count_t readcount = sf_readf_float(in, buffer, bufsamples);

            if (callback != NULL)
            {
                if (frames > 0)
                {
                    processed += readcount;
                    callback(data, (float)processed / (float)frames);
                }
                else
                {
                    callback(data, 0);
                }
            }

            if (readcount != 0)
            {
                deinterlave_stereo(buffer, leftbuf, rightbuf, readcount);
                rb.process(rbbuf, readcount, readcount < bufsamples);
            }

            if (readcount < bufsamples)
            {
                flush = true;
            }

            while (rb.available() > 0)
            {
                int avail = rb.available();
                convcount = rb.retrieve(rbbuf, avail > bufsamples ? bufsamples : avail);
                interleave_stereo(convbuf, leftbuf, rightbuf, convcount);
                sf_writef_float(out, convbuf, convcount);
            }
        }

        success = 0;
    }
    catch (int i)
    {
        success = i;
    }

    if (in != NULL)  sf_close(in);
    if (out != NULL) sf_close(out);
    return success;
}

int change_audio_speed(const char* source, struct buffers *bufs, double speed, bool pitch, double emuldt, void *data, update_progress_cb callback)
{
    try
    {
        return change_audio_speed_libsndfile(source, bufs, speed, pitch, emuldt, data, callback);
    }
    catch (const std::runtime_error &e)
    {
        fprintf(stderr, "%s\n", e.what());
        return -1;
    }
}
