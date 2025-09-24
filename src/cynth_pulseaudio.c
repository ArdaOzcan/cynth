#include "cynth_common.h"
#include <pulse/sample.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct CynthEngine
{
    pa_simple* simple;
} CynthEngine;

CynthError
cynth_engine_write_buffer(CynthEngine* engine, const CynthBuffer* buffer)
{
    int error = 0;
    size_t pcm_data_size =
      buffer->frame_amount * buffer->ss.channels * sizeof(int16_t);
    if (pa_simple_write(engine->simple, buffer->data, pcm_data_size, &error) <
        0) {
        fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(error));
        pa_simple_free(engine->simple);
        return CYNTH_ERROR_WRITE;
    }

    return CYNTH_ERROR_NONE;
}

CynthEngine*
cynth_engine_init(const CynthSampleSpec* ss)
{
    pa_sample_spec pa_ss = { 0 };
    pa_ss.format = (pa_sample_format_t)ss->format;
    pa_ss.channels = ss->channels;
    pa_ss.rate = ss->rate;

    CynthEngine* engine = calloc(1, sizeof(CynthEngine));
    engine->simple = pa_simple_new(NULL,
                                   "CynthDemo",
                                   PA_STREAM_PLAYBACK,
                                   NULL,
                                   "Music",
                                   &pa_ss,
                                   NULL,
                                   NULL,
                                   NULL);
    return engine;
}

void
cynth_engine_deinit(CynthEngine* engine)
{
    pa_simple_free(engine->simple);
}

CynthError
cynth_engine_drain(CynthEngine* engine)
{
    int error = 0;
    if (pa_simple_drain(engine->simple, &error) < 0) {
        fprintf(stderr, "pa_simple_drain() failed: %s\n", pa_strerror(error));
        cynth_engine_deinit(engine);
        return CYNTH_ERROR_DRAIN;
    }

    return CYNTH_ERROR_NONE;
}
