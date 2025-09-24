#ifndef CYNTH_H
#define CYNTH_H

#include "../src/cynth_common.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define CYNTH_PI 3.1415F

#define CYNTH_FREQ_C 261.63F  /* Middle C (C4)*/
#define CYNTH_FREQ_Cs 277.18F /* C#4 / Db4*/
#define CYNTH_FREQ_D 293.66F
#define CYNTH_FREQ_Ds 311.13F /* D#4 / Eb4*/
#define CYNTH_FREQ_E 329.63F
#define CYNTH_FREQ_F 349.23F
#define CYNTH_FREQ_Fs 369.99F /* F#4 / Gb4*/
#define CYNTH_FREQ_G 392.00F
#define CYNTH_FREQ_Gs 415.30F /* G#4 / Ab4*/
#define CYNTH_FREQ_A 440.00F
#define CYNTH_FREQ_As 466.16F /* A#4 / Bb4*/
#define CYNTH_FREQ_B 493.88F

typedef struct CynthEngine CynthEngine;

CynthEngine*
cynth_engine_init(CynthSampleSpec* ss, const char* device);

CynthError
cynth_engine_write_buffer(CynthEngine* engine,
                          const CynthBuffer* buffer,
                          size_t frame_amount);

void
cynth_engine_deinit(CynthEngine* engine);

CynthError
cynth_engine_drain(CynthEngine* engine);

#pragma pack(push, 1)
typedef struct
{
    char ChunkID[4];    /* "RIFF"*/
    uint32_t ChunkSize; /* 36 + Subchunk2Size*/
    char Format[4];     /* "WAVE"*/

    /* fmt Subchunk*/
    char Subchunk1ID[4];    /* "fmt "*/
    uint32_t Subchunk1Size; /* 16 for PCM*/
    uint16_t AudioFormat;   /* PCM = 1*/
    uint16_t NumChannels;   /* 1 = mono, 2 = stereo*/
    uint32_t SampleRate;    /* e.g. 44100*/
    uint32_t ByteRate;      /* SampleRate * NumChannels * BitsPerSample/8*/
    uint16_t BlockAlign;    /* NumChannels * BitsPerSample/8*/
    uint16_t BitsPerSample; /* e.g. 16*/

    /* data Subchunk*/
    char Subchunk2ID[4];    /* "data"*/
    uint32_t Subchunk2Size; /* NumSamples * NumChannels * BitsPerSample/8*/
} CynthWAVHeader;
#pragma pack(pop)

void
cynth_buffer_init(CynthBuffer* buffer,
                  CynthSampleSpec sample_spec,
                  float seconds,
                  void* backing_data);

float
cynth_sine_normalized(float t);

float
cynth_triangle_normalized(float t);

float
cynth_square_normalized(float t);

int
cynth_buffer_add_wave(const CynthBuffer* buffer,
                      CynthEnvelope envelope,
                      size_t start_frame,
                      size_t frame_amount,
                      float (*wave_fn)(float),
                      float frequency,
                      float volume);

void
cynth_write_notes(const CynthBuffer* buffer,
                  CynthNote* notes,
                  size_t note_amount,
                  CynthEnvelope envelope,
                  float (*wave_fn)(float));

void
cynth_buffer_fprint(const CynthBuffer* buffer, FILE* file);

int
cynth_buffer_export_wav(const CynthBuffer* buffer, const char* out_path);

uint32_t
cynth_midi_read_vlq(uint32_t variable_length_number);

void
cynth_midi_import(const void* file_data,
                  size_t file_size,
                  CynthMIDIObject* out_midi);

#endif
