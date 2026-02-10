#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sndfile.h>
#include <soundtouch/SoundTouch.h>
#include <stdexcept>
#include "tools.h"
#include "buffers.h"
#include "audiospeed.h"
#include "cosuplatform.h"
#include "cosuwindow.h"

using namespace soundtouch;
using namespace std;

static sf_count_t sfvio_get_filelen(void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    return bufs->audlast;
}

static sf_count_t sfvio_seek(sf_count_t offset, int whence, void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    return buffers_aud_seek(bufs, offset, whence);
}

static sf_count_t sfvio_read(void *ptr, sf_count_t count, void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    return buffers_aud_get(bufs, ptr, count);
}

static sf_count_t sfvio_write(const void *ptr, sf_count_t count, void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    if (buffers_aud_put(bufs, ptr, count) == 0) return count;
    else return 0;
}

static sf_count_t sfvio_tell(void *vbufs)
{
    struct buffers *bufs = (struct buffers*) vbufs;
    return bufs->audcur;
}

static int change_audio_speed_real(const char* source, struct buffers *bufs, double speed, bool pitch, void *data, update_progress_cb callback)
{
    int success = 1;
    SNDFILE *in = NULL, *out = NULL;
    SF_INFO info = { 0, 0, 0, 0, 0, 0 };
    SF_VIRTUAL_IO sfvio = { &sfvio_get_filelen, &sfvio_seek, &sfvio_read, &sfvio_write, &sfvio_tell };

    float buffer[256] = { 0.0 };
    float convbuf[256] = { 0.0 };
    int bufsamples = 0;

    sf_count_t readcount = 0;
    unsigned int convcount = 0;

    float progress = 0;

    SoundTouch st;
    bool flush = false;

    sf_count_t frames = 0;

    if ((in = sf_open(source, SFM_READ, &info)) == NULL)
    {
        goto sndfclose;
    }

    frames = info.frames;
    info.frames = 0;

    if ((out = sf_open_virtual(&sfvio, SFM_WRITE, &info, bufs)) == NULL)
    {
        goto sndfclose;
    }

    bufsamples = sizeof(buffer) / sizeof(float) / info.channels;

    st.setSetting(SETTING_USE_QUICKSEEK, 1);
    st.setSampleRate(info.samplerate);
    st.setChannels(info.channels);
    if (!pitch) st.setTempoChange((speed - 1.0) * 100.0);
    else st.setRateChange((speed - 1.0) * 100.0);

    while (!flush)
    {
        readcount = sf_readf_float(in, buffer, bufsamples);

        if (callback != NULL)
        {
            if (frames > 0)
                progress += (float) readcount / (float) frames;

            callback(data, progress);
        }

        if (readcount != 0) st.putSamples(buffer, readcount);
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

    success = 0;
sndfclose:
    if (in != NULL)  sf_close(in);
    if (out != NULL) sf_close(out);
    return success;
}

int change_audio_speed(const char* source, struct buffers *bufs, double speed, bool pitch, void *data, update_progress_cb callback)
{
    try
    {
        return change_audio_speed_real(source, bufs, speed, pitch, data, callback);
    }
    catch (const std::runtime_error &e)
    {
        fprintf(stderr, "%s\n", e.what());
        return -1;
    }
}
