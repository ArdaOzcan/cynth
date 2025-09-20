#include "cynth.h"
#include "ccore.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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
caudio_buffer_init(CAudioBuffer* buffer,
                   pa_sample_spec sample_spec,
                   float seconds,
                   Allocator* allocator)
{
    buffer->ss = sample_spec;
    buffer->sample_amount = seconds * sample_spec.rate;
    size_t array_length = buffer->ss.channels * buffer->sample_amount;
    buffer->data = make(int16_t, array_length, allocator);
    memset(buffer->data, 0, sizeof(int16_t) * array_length);
}

// t is 0-1
float
caudio_sine_normalized(float t)
{
    return sinf(t * 2.0f * CAUDIO_PI);
}

float
caudio_triangle_normalized(float t)
{
    t -= (int)t;
    if (t < 0.5f)
        return t * 4.0f - 1.0f;
    else
        return 3.0f - t * 4.0f;
}

float
caudio_square_normalized(float t)
{
    t -= (int)t;
    if (t < 0.5f) {
        return -1.0f;
    }

    return 1.0f;
}

int
caudio_buffer_add_wave(const CAudioBuffer* buffer,
                       size_t start_frame,
                       size_t frame_amount,
                       float (*wave_fn)(float),
                       float frequency,
                       float volume)
{
    size_t frame = 0;
    float phase = 0;
    size_t ramp_samples = (size_t)(0.010f * buffer->ss.rate);
    int16_t* buffer_start = &buffer->data[start_frame];
    for (frame = 0; frame < frame_amount; frame++) {
        phase = frequency * frame / buffer->ss.rate;
        if (phase >= 1.0f)
            phase -= 1.0f;

        float env = 1.0f;
        if (frame < ramp_samples) {
            env = (float)frame / ramp_samples;
        } else if (frame >= frame_amount - ramp_samples) {
            env = (float)(frame_amount - frame) / ramp_samples;
        }

        float s = wave_fn(phase) * env;

        int16_t* left = &buffer_start[frame * 2];
        int16_t* right = &buffer_start[frame * 2 + 1];

        int32_t added_sample = (int32_t)(s * INT16_MAX * volume);
        int32_t new_sample_l =
          clamp_i16(*left + added_sample, INT16_MIN, INT16_MAX);
        int32_t new_sample_r =
          clamp_i16(*right + added_sample, INT16_MIN, INT16_MAX);

        *left = new_sample_l;
        *right = new_sample_r;
    }

    return 0;
}

void
caudio_write_notes(const CAudioBuffer* buffer,
                   CAudioNote* notes,
                   size_t note_amount,
                   float (*wave_fn)(float))
{
    size_t i = 0;
    for (i = 0; i < note_amount; i++) {
        CAudioNote note = notes[i];
        size_t start_frame =
          (size_t)(note.start_time * buffer->ss.rate * buffer->ss.channels);
        caudio_buffer_add_wave(buffer,
                               start_frame,
                               note.duration * buffer->ss.rate,
                               wave_fn,
                               note.frequency,
                               note.volume);
    }
}

void
caudio_buffer_fprint(const CAudioBuffer* buffer, FILE* file)
{
    size_t frame = 0;
    fprintf(file, "[\n");
    for (; frame < buffer->sample_amount; frame++) {
        int16_t new_sample_l = buffer->data[frame * 2];
        fprintf(file, "{\"frame\": %zu, \"sample\": %d}", frame, new_sample_l);
        if (frame != buffer->sample_amount - 1) {
            printf(",\n");
        }
    }
    fprintf(file, "]\n");
}

int
caudio_buffer_export_wav(const CAudioBuffer* buffer, const char* out_path)
{
    size_t pcm_data_size =
      buffer->sample_amount * buffer->ss.channels * sizeof(int16_t);
    CAudioWAVHeader header = { .ChunkID = "RIFF",
                               .ChunkSize = 36 + pcm_data_size,
                               .Format = "WAVE",
                               .Subchunk1ID = "fmt ",
                               .Subchunk1Size = 16,
                               .AudioFormat = 1,
                               .NumChannels = buffer->ss.channels,
                               .SampleRate = buffer->ss.rate,
                               .ByteRate =
                                 buffer->ss.rate * buffer->ss.channels * 16 / 8,
                               .BlockAlign = buffer->sample_amount * 16 / 8,
                               .BitsPerSample = 16,
                               .Subchunk2ID = "data",
                               .Subchunk2Size = pcm_data_size };

    FILE* file = fopen(out_path, "wb");
    if (file == NULL) {
        return 1;
    }

    fwrite(&header, sizeof(header), 1, file);
    fwrite(buffer->data, pcm_data_size, 1, file);
    fclose(file);

    return 0;
}
