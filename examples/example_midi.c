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

static double
freq_to_midi(double freq)
{
    return 69.0 + 12.0 * (log(freq / 440) / log(2.0));
}

static int
freq_to_nearest_midi(double freq)
{
    double m = freq_to_midi(freq);
    return (int)llround(m);
}

static double
cents_from_freq(double freq)
{
    double m = freq_to_midi(freq);
    double nearest = (double)llround(m);
    return (m - nearest) * 100.0;
}

static double
delta_time_to_seconds(CynthMIDIHeader header, uint32_t delta_time)
{
    if (header.division & 0x80) {
        /* Ticks per quarter note */
    } else {
        /* Negative SMPT */
    }
    return delta_time * 0.00033f;
}

void
cynth_midi_track_to_notes(CynthMIDITrackInfo track,
                          CynthMIDIHeader header,
                          CynthNote* notes,
                          size_t* note_amount)
{
    size_t i = 0;
    size_t w = 0;
    uint32_t t = 0;
    for (i = 0; i < array_len(track.events); i++) {
        CynthMIDIEvent evt = track.events[i];
        t += evt.delta_time;
        if (evt.type == CYNTH_MIDI_EVENT_NOTE_ON) {
            size_t j = i;
            uint32_t duration = 0;
            for (; j < array_len(track.events); j++) {
                duration += track.events[j].delta_time;
                if (track.events[j].type == CYNTH_MIDI_EVENT_NOTE_OFF &&
                    track.events[j].note.key == evt.note.key) {
                    /* Found pair event */
                    CynthNote note = { 0 };
                    note.duration = delta_time_to_seconds(header, duration);
                    note.start_time = delta_time_to_seconds(header, t);
                    note.frequency = midi_to_freq(evt.note.key);
                    note.volume = evt.note.velocity / 127.0f;
                    note.volume *= 0.15f;
                    notes[w++] = note;
                    break;
                }
            }
        }
    }

    *note_amount = w;
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
           0.263f * cynth_square_normalized(t * 1.998f / 1.0f) +
           0.14f * cynth_triangle_normalized(t * 3.0f / 1.0f) +
           0.099f * cynth_triangle_normalized(t * 4.01f / 1.0f) +
           0.209f * cynth_triangle_normalized(t * 5.02f / 1.0f) +
           0.02f * cynth_triangle_normalized(t * 6.05f / 1.0f) +
           0.029f * cynth_triangle_normalized(t * 7.07f / 1.0f) +
           0.077f * cynth_triangle_normalized(t * 8.12f / 1.0f) +
           0.017f * cynth_triangle_normalized(t * 9.18f / 1.0f) +
           0.01f * cynth_triangle_normalized(t * 10.25f / 1.0f);
}

typedef struct
{
    double phase;
    float (*wave_fn)(float);
} Oscillator;

typedef struct
{
    Oscillator oscillator;
    const CynthEnvelope* envelope;
    bool is_note_on;
    uint8_t current_note;
    uint32_t frames_since_note_start;
    uint32_t frames_since_note_end;
} Voice;

double
cynth_frame_to_seconds(uint32_t frame, const CynthSampleSpec* ss)
{
    return (double)frame / ss->rate;
}

double
cynth_envelope_get_volume(const CynthEnvelope* envelope,
                          double time_since_note_start,
                          double time_since_note_end)
{
    double env = 0;
    double duration = time_since_note_start - time_since_note_end;
    if (duration < envelope->attack) {
        /* Attack */
        env = (duration / envelope->attack);
    } else if (duration < envelope->decay + envelope->attack) {
        /* Decay */
        double t = (duration - envelope->attack) / envelope->decay;
        env = 1.0 + t * (envelope->sustain - 1.0);
    } else {
        /* Sustain */
        env = envelope->sustain;
    }

    if (time_since_note_end > 0) {
        /* Release */
        env *= 1 - (time_since_note_end / envelope->release);
    }
    if (env < 0)
        env = 0;
    /* CLOG_DEBUG("env: %.2f, t=%f", env, time_since_note_end /
     * envelope->release); */

    return env;
}

double
cynth_oscillator_next_sample(Oscillator* oscillator,
                             double frequency,
                             uint32_t rate)
{
    double sample = oscillator->wave_fn(oscillator->phase);
    oscillator->phase += frequency / rate;
    if (oscillator->phase >= 1.0f)
        oscillator->phase -= 1.0f;

    return sample;
}

void
cynth_voice_add_to_buffer(Voice* voice,
                          CynthBuffer* buffer,
                          uint16_t start_frame,
                          uint16_t frame_amount)
{
    assert(start_frame < frame_amount);

    size_t f = start_frame;
    for (; f < frame_amount; f++) {
        double frequency = midi_to_freq(voice->current_note);
        // CLOG_DEBUG("start delta: %u, end delta: %u",
        //            voice->frames_since_note_start,
        //            voice->frames_since_note_end);
        double env = cynth_envelope_get_volume(
          voice->envelope,
          (double)voice->frames_since_note_start / buffer->ss.rate,
          (double)voice->frames_since_note_end / buffer->ss.rate);

        double sample = env * cynth_oscillator_next_sample(
                                &voice->oscillator, frequency, buffer->ss.rate);

        if (!voice->is_note_on) {
            voice->frames_since_note_end++;
        }
        voice->frames_since_note_start++;

        int16_t* left = &buffer->data[f * 2];
        int16_t* right = &buffer->data[f * 2 + 1];

        double volume = 0.25;
        int32_t added_sample = (int32_t)(sample * INT16_MAX * volume);
        int32_t new_sample_l =
          clamp_i16(*left + added_sample, INT16_MIN, INT16_MAX);
        int32_t new_sample_r =
          clamp_i16(*right + added_sample, INT16_MIN, INT16_MAX);

        *left = new_sample_l;
        *right = new_sample_r;
    }
}

void
cynth_voice_write_to_buffer(Voice* voice,
                            CynthBuffer* buffer,
                            uint16_t start_frame,
                            uint16_t frame_amount)
{
    assert(start_frame < frame_amount);

    size_t f = start_frame;
    for (; f < frame_amount; f++) {
        double frequency = midi_to_freq(voice->current_note);
        // CLOG_DEBUG("start delta: %u, end delta: %u",
        //            voice->frames_since_note_start,
        //            voice->frames_since_note_end);
        double env = cynth_envelope_get_volume(
          voice->envelope,
          (double)voice->frames_since_note_start / buffer->ss.rate,
          (double)voice->frames_since_note_end / buffer->ss.rate);

        double sample = env * cynth_oscillator_next_sample(
                                &voice->oscillator, frequency, buffer->ss.rate);

        if (!voice->is_note_on) {
            voice->frames_since_note_end++;
        }
        voice->frames_since_note_start++;

        buffer->data[2 * f] = sample * INT16_MAX;
        buffer->data[2 * f + 1] = sample * INT16_MAX;
    }
}

void
cynth_voice_note_start(Voice* voice, uint8_t note)
{
    voice->is_note_on = true;
    voice->current_note = note;
    voice->frames_since_note_start = 0;
    voice->frames_since_note_end = 0;
}

void
cynth_voice_note_end(Voice* voice)
{
    voice->is_note_on = false;
    voice->frames_since_note_end = 0;
}

#define SYNTHESIZER_MAX_VOICE 256

typedef struct
{
    Voice voices[SYNTHESIZER_MAX_VOICE];
    uint8_t voice_amount;
} Synthesizer;

void
cynth_synthesizer_note_start(Synthesizer* s,
                             CynthEnvelope* envelope,
                             uint8_t note)
{
    if (s->voice_amount < SYNTHESIZER_MAX_VOICE) {
        Voice v = { 0 };
        v.oscillator = (Oscillator){ 0, cynth_sine_normalized };
        v.envelope = envelope;

        cynth_voice_note_start(&v, note);
        s->voices[s->voice_amount++] = v;
        CLOG_DEBUG("[+]New voice amount: %d", s->voice_amount);
    }
}

void
cynth_synthesizer_note_end(Synthesizer* s, uint8_t note)
{
    size_t v = 0;
    for (; v < s->voice_amount; v++) {
        if (s->voices[v].is_note_on && s->voices[v].current_note == note) {
            cynth_voice_note_end(&s->voices[v]);
            CLOG_DEBUG("Note %d ended.", note);
        }
    }
}

void
cynth_synthesizer_write_to_buffer(Synthesizer* s,
                                  CynthBuffer* buffer,
                                  uint16_t start_frame,
                                  uint16_t frame_amount)
{
    /* Loop must be reversed so we can safely
     * remove elements during the loop. */
    int v = 0;
    for (v = s->voice_amount - 1; v >= 0; v--) {
        Voice* voice = &s->voices[v];
        cynth_voice_add_to_buffer(voice, buffer, start_frame, frame_amount);

        bool release_ended = voice->frames_since_note_end >
                             voice->envelope->release * buffer->ss.rate;
        if (!release_ended)
            continue;

        /* Remove voice */
        if (v != s->voice_amount - 1) {
            Voice temp = s->voices[v];
            s->voices[v] = s->voices[s->voice_amount - 1];
            s->voices[s->voice_amount - 1] = temp;
        }
        s->voice_amount--;
        CLOG_DEBUG("[-]New voice amount: %d", s->voice_amount);
    }
}

void
cynth_buffer_clear(CynthBuffer* buffer)
{
    memset(buffer->data,
           0,
           sizeof(int16_t) * buffer->frame_amount * buffer->ss.channels);
}

void
test(void)
{
    clog_log_level_set(CLOG_LOG_LEVEL_DEBUG);
    size_t size = 0;
    void* file_data = read_file_to_buffer("songs/g7_inversions.mid", &size);
    assert(file_data);

    CynthMIDIObject midi = { 0 };
    cynth_midi_import(file_data, size, &midi);

    Voice voice = { 0 };
    Oscillator oscillator = { 0, cynth_sine_normalized };
    voice.oscillator = oscillator;
    CynthEnvelope envelope = {
        .attack = 1.0f, .decay = 0.5f, .sustain = 0.5f, .release = 0.5f
    };

    voice.envelope = &envelope;

    CynthSampleSpec ss = { .rate = 44100,
                           .format = CYNTH_SAMPLE_S16LE,
                           .channels = 2 };
    CynthEngine* engine = cynth_engine_init(&ss, "default");

    CynthBuffer buffer = { 0 };
    float buffer_time = 0.5f;
    int16_t* backing_data =
      calloc(buffer_time * ss.rate * ss.channels, sizeof(int16_t));
    cynth_buffer_init(&buffer, ss, buffer_time, backing_data);

    uint16_t period = 1024;

    size_t play_time = 15;

    size_t e = 0;
    size_t t = 1;
    assert(t < array_len(midi.tracks));
    for (; e < midi.tracks[t].event_amount; e++) {
        CynthMIDIEvent evt = midi.tracks[t].events[e];
        CLOG_DEBUG("Event: type(%d), note.key(%d), delta(%lu)",
                   evt.type,
                   evt.note.key,
                   evt.delta_time);
    }

    int16_t* wav_buffer =
      calloc(play_time * ss.rate * ss.channels, sizeof(int16_t));
    Synthesizer s = { 0 };
    double last_evt_start = 0;
    size_t p = 0;
    e = 0;
    uint32_t frame = 0;
    for (; p < play_time * ss.rate / period; p++) {
        double period_start_time = ((double)p * period) / ss.rate;
        double period_end_time = ((p + 1.0) * period) / ss.rate;
        frame = p * period;

        while (e < midi.tracks[t].event_amount) {
            CynthMIDIEvent evt = midi.tracks[t].events[e];
            double curr_evt_start =
              last_evt_start +
              delta_time_to_seconds(midi.header, evt.delta_time);

            if (curr_evt_start > period_end_time)
                break;

            if (evt.type == CYNTH_MIDI_EVENT_NOTE_ON) {
                cynth_synthesizer_note_start(&s, &envelope, evt.note.key);
            } else if (evt.type == CYNTH_MIDI_EVENT_NOTE_OFF) {
                cynth_synthesizer_note_end(&s, evt.note.key);
            }
            last_evt_start = curr_evt_start;
            e++;
        }

        /* for (; event_start_time < period_end_time &&
               e < midi.tracks[t].event_amount;
             e++) {
            CynthMIDIEvent evt = midi.tracks[t].events[e];
            if (evt.type == CYNTH_MIDI_EVENT_NOTE_ON) {
                cynth_synthesizer_note_start(&s, &envelope, evt.note.key);
            } else if (evt.type == CYNTH_MIDI_EVENT_NOTE_OFF) {
                cynth_synthesizer_note_end(&s, evt.note.key);
            }
            event_start_time +=
              delta_time_to_seconds(midi.header, evt.delta_time);
        } */

        cynth_buffer_clear(&buffer);
        cynth_synthesizer_write_to_buffer(&s, &buffer, 0, period);
        cynth_engine_write_buffer(engine, &buffer, period);

        /* For wav export */
        memcpy(wav_buffer + p * period * ss.channels,
               buffer.data,
               period * ss.channels * sizeof(int16_t));
    }

    CLOG_DEBUG("Period amount=%zu", p);
    buffer.data = wav_buffer;
    buffer.frame_amount = play_time * ss.rate;

    cynth_buffer_export_wav(&buffer, "out/sine.wav");

    cynth_engine_drain(engine);
    cynth_engine_deinit(engine);
}

int
read_midi_and_play(void)
{
    clog_log_level_set(CLOG_LOG_LEVEL_DEBUG);
    size_t size = 0;
    void* file_data = read_file_to_buffer("songs/DancingQueen.mid", &size);
    assert(file_data);

    CynthMIDIObject midi = { 0 };
    cynth_midi_import(file_data, size, &midi);

    size_t t = 0;
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
    cynth_buffer_init(&buffer, ss, buffer_duration, data);

    CynthEnvelope env = {
        .attack = 0.0125f, .decay = 0.5f, .sustain = 0.5f, .release = 0.5f
    };

    cynth_write_notes(&buffer, notes, note_amount, env, wave_custom);
    cynth_filter_delay(&buffer, 1.32f, 0.75f);
    cynth_filter_delay(&buffer, 1.0f, 0.125f);
    cynth_filter_delay(&buffer, 1.5f, 0.025f);
    cynth_filter_delay(&buffer, 2.0f, 0.0125f);

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
