#include "clog.h"
#include "cynth_common.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct CynthEngine
{
    snd_pcm_t* pcm_handle;
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_sw_params_t* sw_params;
    snd_pcm_uframes_t frames;
    snd_pcm_format_t format;
    unsigned int rate;
    int channels;
} CynthEngine;

static snd_pcm_format_t
sample_format_cynth_to_alsa(CynthSampleFormat cynth)
{
    switch (cynth) {
        case CYNTH_SAMPLE_U8:
            return SND_PCM_FORMAT_U8;
        case CYNTH_SAMPLE_ALAW:
            return SND_PCM_FORMAT_A_LAW;
        case CYNTH_SAMPLE_ULAW:
            return SND_PCM_FORMAT_MU_LAW;
        case CYNTH_SAMPLE_S16LE:
            return SND_PCM_FORMAT_S16_LE;
        case CYNTH_SAMPLE_S16BE:
            return SND_PCM_FORMAT_S16_BE;
        case CYNTH_SAMPLE_FLOAT32LE:
        case CYNTH_SAMPLE_FLOAT32BE:
        case CYNTH_SAMPLE_S32LE:
        case CYNTH_SAMPLE_S32BE:
        case CYNTH_SAMPLE_S24LE:
        case CYNTH_SAMPLE_S24BE:
        case CYNTH_SAMPLE_S24_32LE:
        case CYNTH_SAMPLE_S24_32BE:
        case CYNTH_SAMPLE_MAX:
        case CYNTH_SAMPLE_INVALID:
            return SND_PCM_FORMAT_UNKNOWN;
            break;
    }
}

CynthEngine*
cynth_engine_init(CynthSampleSpec* ss, const char* device)
{
    CynthEngine* engine = calloc(1, sizeof(CynthEngine));
    int retval          = 0;
    if ((retval = snd_pcm_open(
           &engine->pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        CLOG_ERROR("ERROR: Can't open \"%s\" PCM device. %s",
                   device,
                   snd_strerror(retval));
        return NULL;
    }

    snd_pcm_hw_params_malloc(&engine->hw_params);
    snd_pcm_hw_params_any(engine->pcm_handle, engine->hw_params);

    snd_pcm_hw_params_set_access(
      engine->pcm_handle, engine->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(engine->pcm_handle,
                                 engine->hw_params,
                                 sample_format_cynth_to_alsa(ss->format));
    snd_pcm_hw_params_set_channels(
      engine->pcm_handle, engine->hw_params, ss->channels);
    snd_pcm_hw_params_set_rate_near(
      engine->pcm_handle, engine->hw_params, &ss->rate, 0);

    unsigned int buffer_time = 20000; // 20ms buffer
    unsigned int period_time = 5000;  // 5ms period
    int dir                  = 0;
    snd_pcm_hw_params_set_buffer_time_near(
      engine->pcm_handle, engine->hw_params, &buffer_time, &dir);
    snd_pcm_hw_params_set_period_time_near(
      engine->pcm_handle, engine->hw_params, &period_time, &dir);

    if ((retval = snd_pcm_hw_params(engine->pcm_handle, engine->hw_params)) <
        0) {
        CLOG_ERROR("ERROR: Can't set hardware params. %s",
                   snd_strerror(retval));
        return NULL;
    }

    snd_pcm_hw_params_free(engine->hw_params);

    snd_pcm_hw_params_get_period_size(engine->hw_params, &engine->frames, 0);

    snd_pcm_prepare(engine->pcm_handle);

    return engine;
}

uint32_t
cynth_engine_get_period_size(CynthEngine* engine)
{
    snd_pcm_uframes_t period_size;
    snd_pcm_hw_params_get_period_size(engine->hw_params, &period_size, 0);
    return period_size;
}

CynthError
cynth_engine_write_buffer(CynthEngine* engine,
                          const CynthBuffer* buffer,
                          size_t frame_amount)
{
    snd_pcm_state_t state         = snd_pcm_state(engine->pcm_handle);
    snd_pcm_sframes_t frames_left = frame_amount;
    int16_t* ptr                  = buffer->data;

    snd_pcm_uframes_t period_size;
    snd_pcm_hw_params_get_period_size(engine->hw_params, &period_size, 0);

    while (frames_left > 0) {
        snd_pcm_sframes_t retval =
          snd_pcm_writei(engine->pcm_handle,
                         ptr,
                         period_size > frames_left ? frames_left : period_size);
        // printf("Write i retval: %ld\n", retval);

        if (retval == -EPIPE) {
            CLOG_WARNING("Buffer underrun: %s", snd_strerror(retval));
            snd_pcm_recover(engine->pcm_handle, retval, false);
            continue;
        } else if (retval < 0) {
            CLOG_ERROR("Can't write to PCM device. %s", snd_strerror(retval));
            return CYNTH_ERROR_WRITE;
        }

        ptr += retval * buffer->ss.channels;
        frames_left -= retval;
    }

    return CYNTH_ERROR_NONE;
}

void
cynth_engine_deinit(CynthEngine* engine)
{
    snd_pcm_close(engine->pcm_handle);
}

CynthError
cynth_engine_drain(CynthEngine* engine)
{
    int retval = snd_pcm_drain(engine->pcm_handle);
    if (retval < 0) {
        fprintf(stderr, "ERROR: ALSA Draining. %s\n", snd_strerror(retval));
    }
    return CYNTH_ERROR_NONE;
}

CynthError
cynth_engine_drop(CynthEngine* engine)
{
    int retval = snd_pcm_drop(engine->pcm_handle);
    if (retval < 0) {
        fprintf(stderr, "ERROR: ALSA Dropping. %s\n", snd_strerror(retval));
    }
    return CYNTH_ERROR_NONE;
}
