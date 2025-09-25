#include "cynth.h" #include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
/* Windows implementation*/
#else
#include "cynth_alsa.c"
#endif

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
cynth_buffer_init(CynthBuffer* buffer,
                  CynthSampleSpec sample_spec,
                  float seconds,
                  void* backing_data)
{
    buffer->ss = sample_spec;
    buffer->frame_amount = seconds * sample_spec.rate;
    buffer->data = backing_data;
    size_t data_length = buffer->ss.channels * buffer->frame_amount;
    memset(buffer->data, 0, sizeof(int16_t) * data_length);
}

// t is 0-1
float
cynth_sine_normalized(float t)
{
    return sinf(t * 2.0f * CYNTH_PI);
}

float
cynth_triangle_normalized(float t)
{
    t -= (int)t;
    if (t < 0.5f)
        return t * 4.0f - 1.0f;
    else
        return 3.0f - t * 4.0f;
}

float
cynth_square_normalized(float t)
{
    t -= (int)t;
    if (t < 0.5f) {
        return -1.0f;
    }

    return 1.0f;
}

int
cynth_buffer_add_wave(const CynthBuffer* buffer,
                      CynthEnvelope envelope,
                      size_t start_frame,
                      size_t frame_amount,
                      float (*wave_fn)(float),
                      float frequency,
                      float volume)
{
    float phase = 0;
    size_t attack_frames = (size_t)(envelope.attack * buffer->ss.rate);
    size_t decay_frames = (size_t)(envelope.decay * buffer->ss.rate);
    size_t release_frames = (size_t)(envelope.release * buffer->ss.rate);

    float release_max = envelope.sustain;
    if (attack_frames > frame_amount) {
        float attack_t = (float)frame_amount / attack_frames;
        release_max = attack_t;
    } else if (attack_frames + decay_frames > frame_amount) {
        float decay_t = (float)(frame_amount - attack_frames) / decay_frames;
        release_max = 1.0f + decay_t * (envelope.sustain - 1.0f);
    }

    int16_t* buffer_start = &buffer->data[start_frame * 2];
    size_t frame = 0;
    for (frame = 0; frame < frame_amount + release_frames; frame++) {
        phase = frequency * frame / buffer->ss.rate;
        if (phase >= 1.0f)
            phase -= 1.0f;

        float env = envelope.sustain;
        if (frame > frame_amount) {
            float release_t = (float)(frame - frame_amount) / release_frames;
            env = release_max * (1.0f - release_t);
        } else if (frame < attack_frames) {
            float attack_t = (float)frame / attack_frames;
            /* Linear */
            env = attack_t;
        } else if (frame <= attack_frames + decay_frames) {
            float decay_t = (float)(frame - attack_frames) / decay_frames;
            env = 1.0f + decay_t * (envelope.sustain - 1.0f);
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
cynth_write_notes(const CynthBuffer* buffer,
                  CynthNote* notes,
                  size_t note_amount,
                  CynthEnvelope envelope,
                  float (*wave_fn)(float))
{
    size_t i = 0;
    for (i = 0; i < note_amount; i++) {
        printf("Note %zu\n", i);
        CynthNote note = notes[i];
        size_t start_frame = (size_t)(note.start_time * buffer->ss.rate);
        cynth_buffer_add_wave(buffer,
                              envelope,
                              start_frame,
                              note.duration * buffer->ss.rate,
                              wave_fn,
                              note.frequency,
                              note.volume);
    }
}

void
cynth_buffer_fprint(const CynthBuffer* buffer, FILE* file)
{
    size_t frame = 0;
    fprintf(file, "[\n");
    for (; frame < buffer->frame_amount; frame++) {
        int16_t new_sample_l = buffer->data[frame * 2];
        fprintf(file, "{\"frame\": %zu, \"sample\": %d}", frame, new_sample_l);
        if (frame != buffer->frame_amount - 1) {
            printf(",\n");
        }
    }
    fprintf(file, "]\n");
}

int
cynth_buffer_export_wav(const CynthBuffer* buffer, const char* out_path)
{
    size_t pcm_data_size =
      buffer->frame_amount * buffer->ss.channels * sizeof(int16_t);

    CynthWAVHeader header = { .ChunkID = "RIFF",
                              .ChunkSize = 36 + pcm_data_size,
                              .Format = "WAVE",
                              .Subchunk1ID = "fmt ",
                              .Subchunk1Size = 16,
                              .AudioFormat = 1,
                              .NumChannels = buffer->ss.channels,
                              .SampleRate = buffer->ss.rate,
                              .ByteRate =
                                buffer->ss.rate * buffer->ss.channels * 16 / 8,
                              .BlockAlign = buffer->frame_amount * 16 / 8,
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
