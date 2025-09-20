#pragma once

#include "ccore.h"
#include <assert.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <stdint.h>

#define CAUDIO_PI 3.1415F

#define CAUDIO_FREQ_C 261.63F  // Middle C (C4)
#define CAUDIO_FREQ_Cs 277.18F // C#4 / Db4
#define CAUDIO_FREQ_D 293.66F
#define CAUDIO_FREQ_Ds 311.13F // D#4 / Eb4
#define CAUDIO_FREQ_E 329.63F
#define CAUDIO_FREQ_F 349.23F
#define CAUDIO_FREQ_Fs 369.99F // F#4 / Gb4
#define CAUDIO_FREQ_G 392.00F
#define CAUDIO_FREQ_Gs 415.30F // G#4 / Ab4
#define CAUDIO_FREQ_A 440.00F
#define CAUDIO_FREQ_As 466.16F // A#4 / Bb4
#define CAUDIO_FREQ_B 493.88F

#pragma pack(push, 1)
typedef struct
{
    // RIFF Chunk
    char ChunkID[4];    // "RIFF"
    uint32_t ChunkSize; // 36 + Subchunk2Size
    char Format[4];     // "WAVE"

    // fmt Subchunk
    char Subchunk1ID[4];    // "fmt "
    uint32_t Subchunk1Size; // 16 for PCM
    uint16_t AudioFormat;   // PCM = 1
    uint16_t NumChannels;   // 1 = mono, 2 = stereo
    uint32_t SampleRate;    // e.g. 44100
    uint32_t ByteRate;      // SampleRate * NumChannels * BitsPerSample/8
    uint16_t BlockAlign;    // NumChannels * BitsPerSample/8
    uint16_t BitsPerSample; // e.g. 16

    // data Subchunk
    char Subchunk2ID[4];    // "data"
    uint32_t Subchunk2Size; // NumSamples * NumChannels * BitsPerSample/8
} CAudioWAVHeader;
#pragma pack(pop)

typedef struct
{
    int16_t* data;
    size_t sample_amount;
    pa_sample_spec ss;
} CAudioBuffer;

typedef struct
{
    float attack;
    float decay;
    float sustain;
    float release;
} CAudioEnvelope;

typedef struct
{
    float frequency;
    float start_time;
    float duration;
    float volume;
} CAudioNote;

void
caudio_buffer_init(CAudioBuffer* buffer,
                   pa_sample_spec sample_spec,
                   float seconds,
                   Allocator* allocator);

float
caudio_sine_normalized(float t);

float
caudio_triangle_normalized(float t);

float
caudio_square_normalized(float t);

int
caudio_buffer_add_wave(const CAudioBuffer* buffer,
                       size_t start_frame,
                       size_t frame_amount,
                       float (*wave_fn)(float),
                       float frequency,
                       float volume);

void
caudio_write_notes(const CAudioBuffer* buffer,
                   CAudioNote* notes,
                   size_t note_amount,
                   float (*wave_fn)(float));

void
caudio_buffer_fprint(const CAudioBuffer* buffer, FILE* file);

int
caudio_buffer_export_wav(const CAudioBuffer* buffer, const char* out_path);
