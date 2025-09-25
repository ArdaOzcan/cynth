#ifndef CYNTH_H
#define CYNTH_H

#include "../src/cynth_common.h"

#include <assert.h>
#include <stdbool.h>
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

#define CYNTH_SYNTHESIZER_MAX_VOICE 32

typedef struct
{
    float attack;
    float decay;
    float sustain;
    float release;
} CynthEnvelope;

typedef struct
{
    float frequency;
    float start_time;
    float duration;
    float volume;
} CynthNote;

typedef enum
{
    CYNTH_MIDI_FORMAT_SINGLE = 0,
    CYNTH_MIDI_FORMAT_VERTICAL = 1,
    CYNTH_MIDI_FORMAT_HORIZONTAL = 2,
} CynthMIDIFormat;

typedef struct
{
    CynthMIDIFormat format;
    uint16_t num_tracks;
    uint16_t division;
	uint32_t tempo;
} CynthMIDIHeader;

typedef struct
{
    uint8_t key;
    uint8_t velocity;
} CynthMIDIEvent_Note;

typedef enum
{
    CYNTH_MIDI_EVENT_UNKNOWN = -1,
    CYNTH_MIDI_EVENT_NOTE_ON,
    CYNTH_MIDI_EVENT_NOTE_OFF,
} CynthMIDIEventType;

typedef struct
{
    CynthMIDIEventType type;
    uint32_t delta_time;
    union
    {
        CynthMIDIEvent_Note note;
    };
} CynthMIDIEvent;

typedef struct
{
    size_t event_amount;
    CynthMIDIEvent* events;
} CynthMIDITrackInfo;

typedef struct
{
    CynthMIDIHeader header;
    CynthMIDITrackInfo* tracks;
} CynthMIDIObject;


typedef struct
{
    double phase;
    float (*wave_fn)(float);
} CynthOscillator;

typedef struct
{
    bool is_note_on;
    uint8_t current_note;
    const CynthEnvelope* envelope;
    uint32_t frames_since_note_start;
    uint32_t frames_since_note_end;
    CynthOscillator oscillator;
} CynthVoice;

typedef struct
{
    uint8_t voice_amount;
    float (*wave_fn)(float);
    const CynthEnvelope* envelope;
    CynthVoice voices[CYNTH_SYNTHESIZER_MAX_VOICE];
} CynthSynthesizer;

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

/* Wave functions */
float
cynth_sine_normalized(float t);

float
cynth_triangle_normalized(float t);

float
cynth_square_normalized(float t);

/* Helper functions */
double
cynth_midi_time_to_seconds(CynthMIDIHeader header, uint32_t delta_time);

double
cynth_frames_to_seconds(uint32_t frames, const CynthSampleSpec* ss);

double
cynth_envelope_get_volume(const CynthEnvelope* envelope,
                          double time_since_note_start,
                          double time_since_note_end);

/* Buffer */
void
cynth_buffer_init(CynthBuffer* buffer,
                  CynthSampleSpec sample_spec,
                  float seconds,
                  void* backing_data);
int
cynth_buffer_add_wave(const CynthBuffer* buffer,
                      CynthEnvelope envelope,
                      size_t start_frame,
                      size_t frame_amount,
                      float (*wave_fn)(float),
                      float frequency,
                      float volume);

void
cynth_buffer_write_notes(const CynthBuffer* buffer,
                         CynthNote* notes,
                         size_t note_amount,
                         CynthEnvelope envelope,
                         float (*wave_fn)(float));

void
cynth_buffer_fprint(const CynthBuffer* buffer, FILE* file);

int
cynth_buffer_export_wav(const CynthBuffer* buffer, const char* out_path);

void
cynth_buffer_clear(const CynthBuffer* buffer);

/* Voice */

void
cynth_voice_add_to_buffer(CynthVoice* voice,
                          CynthBuffer* buffer,
                          uint16_t start_frame,
                          uint16_t frame_amount);

void
cynth_voice_write_to_buffer(CynthVoice* voice,
                            CynthBuffer* buffer,
                            uint16_t start_frame,
                            uint16_t frame_amount);

void
cynth_voice_note_start(CynthVoice* voice, uint8_t note);

void
cynth_voice_note_end(CynthVoice* voice);

void
cynth_voice_init(CynthVoice* voice,
                 float (*wave_fn)(float),
                 const CynthEnvelope* envelope);

/* Synthesizer */
void
cynth_synthesizer_init(CynthSynthesizer* s,
                       float (*wave_fn)(float),
                       const CynthEnvelope* envelope);

void
cynth_synthesizer_play_midi_events(CynthSynthesizer* s,
                                   CynthEngine* engine,
                                   CynthSampleSpec ss,
                                   CynthMIDIHeader header,
                                   CynthMIDIEvent* events,
                                   size_t event_amount);

void
cynth_synthesizer_note_start(CynthSynthesizer* s,
                             const CynthEnvelope* envelope,
                             uint8_t note);

void
cynth_synthesizer_note_end(CynthSynthesizer* s, uint8_t note);

void
cynth_synthesizer_write_to_buffer(CynthSynthesizer* s,
                                  CynthBuffer* buffer,
                                  uint16_t start_frame,
                                  uint16_t frame_amount);

/* MIDI */
void
cynth_midi_import(const void* file_data,
                  size_t file_size,
                  CynthMIDIObject* out_midi);

#endif
