#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mpg123.h>
#include <lame/lame.h>
#include <sndfile.h>
#include <soundtouch/SoundTouch.h>
#include <stdexcept>
#include "tools.h"
#include "buffers.h"
#include "audiospeed.h"
#include "cosuplatform.h"

using namespace soundtouch;
using namespace std;

int change_mp3_speed(const char* source, struct buffers *bufs, double speed, bool pitch, float *progress)
{
    *progress = 0;
    mpg123_handle *mh = NULL;
    struct mpg123_frameinfo fi;
    SoundTouch st;
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

    int bufsizesample = 0;
    size_t samplecount = 0;
    size_t done = 0;
    size_t wanted = 0;
    unsigned int fulllength = 0;
    bool flush = false;

    int success = 1;
    try
    {
        err = mpg123_init();
        if (err != MPG123_OK || (mh = mpg123_new(NULL, &err)) == NULL)
        {
            printerr("Failed initalizing mpg123");
            throw -99;
        }

        mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT|MPG123_GAPLESS, 0.);

        if (mpg123_open(mh, source) != MPG123_OK || mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK)
        {
            throw -99;
        }

        mpg123_scan(mh);
        fulllength = mpg123_length(mh);

        mpg123_info(mh, &fi);
        mpg123_format_none(mh);
        mpg123_format(mh, rate, channels, encoding);
        buffer_size = mpg123_outblock(mh);
        buffer = (float*) malloc(buffer_size);
        convbuf = (float*) malloc((convbuf_size = buffer_size));
        if (buffer == NULL || convbuf == NULL) throw -99;

        st.setSampleRate(rate);
        st.setChannels(channels);
        if (!pitch) st.setTempoChange((speed - 1.0) * 100.0);
        else st.setRateChange((speed - 1.0) * 100.0);

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
            printerr("LAME failed initialization");
            throw -99;
        }

        bufsizesample = convbuf_size / sizeof(float) / channels;

        while (1)
        {
            if (err == MPG123_DONE) flush = true;

            if (!flush)
            {
                err = mpg123_read(mh, (unsigned char*) buffer, buffer_size, &done);
                samplecount = (done/sizeof(float))/channels;
                *progress += (float) samplecount / (float) fulllength;

                if (err == MPG123_OK || err == MPG123_DONE)
                {
                    st.putSamples(buffer, samplecount);
                }
                else
                {
                    fprintf(stderr, "Error while decoding a mp3: %d\n", err);
                    throw -99;
                }
            }
            else
            {
                st.flush();
            }

            do
            {
                samplecount = st.receiveSamples(convbuf, bufsizesample);
                if (samplecount == 0) continue;
                wanted = 1.25 * samplecount + 7200;
                if (mp3buf_size < wanted)
                {
                    unsigned char* tmp = (unsigned char*) realloc(mp3buf, wanted);
                    if (tmp == NULL)
                    {
                        printerr("Failed allocating memory");
                        throw -99;
                    }

                    mp3buf = tmp;
                    mp3buf_size = wanted;
                }

                lamerr = lame_encode_buffer_interleaved_ieee_float(gfp, convbuf, samplecount, mp3buf, mp3buf_size);
                if (lamerr < 0)
                {
                    printerr("LAME encoding failed");
                    throw -99;
                }

                if (buffers_aud_put(bufs, mp3buf, lamerr) != 0)
                {
                    printerr("Audio Buffer failed");
                    throw -99;
                }
            }
            while (samplecount != 0);

            if (flush) break;
        }


        lamerr = lame_encode_flush(gfp, mp3buf, mp3buf_size);
        if (lamerr > 0)
        {
            if (buffers_aud_put(bufs, mp3buf, lamerr) != 0)
            {
                printerr("Audio Buffer failed");
                throw -99;
            }
        }

        /*lamerr = lame_encode_flush_nogap(gfp, mp3buf, mp3buf_size);
        if (lamerr > 0)
        {
           if (buffers_aud_put(bufs, mp3buf, lamerr) != 0)
           {
              goto cleanup;
           }
        }*/
        // seems to be useless

        lamerr = lame_get_lametag_frame(gfp, mp3buf, mp3buf_size);
        if ((unsigned long) lamerr < mp3buf_size)
        {
            memcpy(bufs->audbuf, mp3buf, lamerr);
        }
        else
        {
            printerr("Failed finishing a mp3");
        }

        success = 0;
        *progress = 1;
    }
    catch (int i)
    {
        success = i;
    }
    free(buffer);
    free(convbuf);
    free(mp3buf);
    if (gfp) lame_close(gfp);
    if (mh)
    {
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
    }
    return success;
}

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

int change_audio_speed_libsndfile(const char* source, struct buffers *bufs, double speed, bool pitch, float *progress)
{
    *progress = 0;
    int success = 1;
    SNDFILE *in = NULL, *out = NULL;
    SF_INFO info = { 0, 0, 0, 0, 0, 0 };
    SF_VIRTUAL_IO sfvio = { &sfvio_get_filelen, &sfvio_seek, &sfvio_read, &sfvio_write, &sfvio_tell };

    float buffer[1024] = { 0.0 };
    float convbuf[1024] = { 0.0 };
    int bufsamples = 0;

    sf_count_t readcount = 0;
    unsigned int convcount = 0;

    SoundTouch st;
    bool flush = false;

    if ((in = sf_open(source, SFM_READ, &info)) == NULL || (out = sf_open_virtual(&sfvio, SFM_WRITE, &info, bufs)) == NULL)
    {
        goto sndfclose;
    }

    bufsamples = 1024 / info.channels;

    st.setSampleRate(info.samplerate);
    st.setChannels(info.channels);
    if (!pitch) st.setTempoChange((speed - 1.0) * 100.0);
    else st.setRateChange((speed - 1.0) * 100.0);

    while (!flush)
    {
        readcount = sf_read_float(in, buffer, 1024);
        *progress += (float) (readcount / info.channels) / (float) info.frames;

        if (readcount != 0) st.putSamples(buffer, readcount / info.channels);
        else
        {
            st.flush();
            flush = true;
        }

        do
        {
            convcount = st.receiveSamples(convbuf, bufsamples);
            sf_writef_float(out, convbuf, convcount);
        }
        while (convcount != 0);
    }

    *progress = 1;
    success = 0;
sndfclose:
    if (in != NULL)  sf_close(in);
    if (out != NULL) sf_close(out);
    return success;
}

int change_audio_speed(const char* source, struct buffers *bufs, double speed, bool pitch, float *progress)
{
    try
    {
        if (endswith(source, ".mp3"))
        {
            return change_mp3_speed(source, bufs, speed, pitch, progress);
            // using mpg123/lame backend exclusively to make bug fixing easier
            // and there are some distros that has libsndfile without mp3 support since it's only released recently
            // libsndfile also uses mpg123/lame anyway
        }
        else return change_audio_speed_libsndfile(source, bufs, speed, pitch, progress);
    }
    catch (const std::runtime_error &e)
    {
        fprintf(stderr, "%s\n", e.what());
        return -1;
    }
}
