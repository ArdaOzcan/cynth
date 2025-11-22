#include "ccore.h"
#include "clog.h"
#include "cynth.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char*
read_file_to_buffer(const char* filename, size_t* file_size)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc(*file_size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, *file_size, file) != *file_size) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return buffer;
};

static double
midi_to_freq(double midi)
{
    return 440 * pow(2.0, (midi - 69.0) / 12.0);
}

static int16_t
clamp_i16(int32_t val, int16_t min, int16_t max)
{
    if (val < min)
        return min;
    if (val > max)
        return max;
    return val;
}

void
cynth_filter_delay(const CynthBuffer* buffer, float offset_as_sec, float volume)
{
    size_t frame = 0;
    size_t offset_as_frame = offset_as_sec * buffer->ss.rate;
    for (frame = buffer->frame_amount - offset_as_frame; frame > 0; --frame) {
        int16_t* left = &buffer->data[frame * 2 + offset_as_frame];
        int16_t* right = &buffer->data[frame * 2 + offset_as_frame + 1];

        int32_t added_sample_l = volume * buffer->data[frame * 2];
        int32_t added_sample_r = volume * buffer->data[frame * 2 + 1];
        int32_t new_sample_l =
          clamp_i16(*left + added_sample_l, INT16_MIN, INT16_MAX);
        int32_t new_sample_r =
          clamp_i16(*right + added_sample_r, INT16_MIN, INT16_MAX);

        *left = new_sample_l;
        *right = new_sample_r;
    }
}

float
wave_custom(float t)
{
    return 1.0f * cynth_triangle_normalized(t * 1.0f / 1.0f) +
           0.084f * cynth_triangle_normalized(t * 2.0f / 1.0f) +
           0.016f * cynth_triangle_normalized(t * 3.0f / 1.0f) +
           0.013f * cynth_triangle_normalized(t * 4.0f / 1.0f);
    0.010f * cynth_triangle_normalized(t * 5.0f / 1.0f);
    0.005f * cynth_triangle_normalized(t * 6.0f / 1.0f);
}

void
test(void)
{
    clog_log_level_set(CLOG_LOG_LEVEL_INFO);

    size_t size = 0;
    void* file_data = read_file_to_buffer("songs/BohemianRhapsody.mid", &size);
    assert(file_data);

    CynthMIDIObject midi = { 0 };
    cynth_midi_import(file_data, size, &midi);

    CynthEnvelope envelope = {
        .attack = 0.015f, .decay = 0.05f, .sustain = 0.5f, .release = 0.3f
    };
    CynthSynthesizer synthesizer = { 0 };
    cynth_synthesizer_init(&synthesizer, wave_custom, 1.0f, envelope);

    CynthSampleSpec ss = { .rate = 44100,
                           .format = CYNTH_SAMPLE_S16LE,
                           .channels = 2 };
    CynthEngine* engine = cynth_engine_init(&ss, "default");

    size_t t = 0;
    assert(t < array_len(midi.tracks));
    cynth_synthesizer_play_midi_events(&synthesizer,
                                       engine,
                                       ss,
                                       midi.header,
                                       midi.tracks[t].events,
                                       midi.tracks[t].event_amount);

    CLOG_INFO("Draining...");
    cynth_engine_drain(engine);
    CLOG_INFO("Drained.");
    cynth_engine_deinit(engine);
}

int
read_midi_and_play(void)
{
    clog_log_level_set(CLOG_LOG_LEVEL_DEBUG);
    size_t size = 0;
    void* file_data = read_file_to_buffer("songs/IstiklalMarsi.mid", &size);
    assert(file_data);

    CynthMIDIObject midi = { 0 };
    cynth_midi_import(file_data, size, &midi);

    size_t t = 9;
    CynthNote* notes =
      calloc(midi.tracks[t].event_amount / 2, sizeof(CynthNote));
    size_t note_amount = 0;
    cynth_midi_track_to_notes(midi.tracks[t], midi.header, notes, &note_amount);

    CynthEngine* engine;
    CynthSampleSpec ss = { .format = CYNTH_SAMPLE_S16LE,
                           .channels = 2,
                           .rate = 44100 };

    engine = cynth_engine_init(&ss, "default");

    float buffer_duration = notes[note_amount - 1].start_time +
                            notes[note_amount - 1].duration + 2.0f;
    int16_t* data =
      calloc(buffer_duration * ss.rate * ss.channels, sizeof(int16_t));

    CynthBuffer buffer = { 0 };
    cynth_buffer_init(&buffer, ss, buffer_duration * ss.rate, data);

    CynthEnvelope env = {
        .attack = 0.0125f, .decay = 0.75f, .sustain = 0.5f, .release = 0.1f
    };

    cynth_buffer_write_notes(
      &buffer, notes, note_amount, env, cynth_square_normalized);
    cynth_filter_delay(&buffer, 0.66f, 0.75f);
    cynth_filter_delay(&buffer, 1.32f, 0.125f);
    cynth_filter_delay(&buffer, 1.98f, 0.025f);

    if (cynth_buffer_export_wav(&buffer, "out/DancingQueen.wav") != 0) {
        CLOG_ERROR("WAV could not be exported.");
        cynth_engine_deinit(engine);
        return 1;
    }

    CynthError error = { 0 };
    error = cynth_engine_write_buffer(engine, &buffer, buffer.frame_amount);
    if (error != CYNTH_ERROR_NONE) {
        cynth_engine_deinit(engine);
        return 1;
    }
    printf("Write ended.\n");

    printf("Draining.\n");
    error = cynth_engine_drain(engine);
    if (error != CYNTH_ERROR_NONE) {
        cynth_engine_deinit(engine);
        return 1;
    }

    printf("Drain ended.\n");

    cynth_engine_deinit(engine);
    return 0;
}

int
main(void)
{
    test();
}
