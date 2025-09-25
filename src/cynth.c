#include "cynth.h"
#include <math.h>
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

/* t is a normalized parameter between 0-1 for the following functions */
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

double
cynth_midi_time_to_seconds(CynthMIDIHeader header, uint32_t delta_time)
{
    if (header.division & 0x8000) {
        /* Negative SMPT */
        int8_t smpte = (int8_t)(header.division & 0x7F00);
        uint8_t ticks_per_frame = header.division & 0x00FF;
        /* CLOG_INFO("Division: 0b%016b. SMPTE: %d, tpf: %u",
                  header.division,
                  smpte,
                  ticks_per_frame); */
    } else {
        /* Ticks per quarter note */
        uint16_t ticks_per_quarter_note = header.division & 0x7FFF;
        /* CLOG_INFO("Division: Ticks per quarter note: %u",
                  ticks_per_quarter_note); */
        if (ticks_per_quarter_note == 0) {
            CLOG_WARNING("Ticks per quarter note is zero.");
            return 0;
        }

        double seconds_per_quarter = (double)header.tempo / 1e6;
        double quarter_notes = (double)delta_time / ticks_per_quarter_note;
        return seconds_per_quarter * quarter_notes;
    }

    return delta_time * 0.0066f;
}

double
cynth_frames_to_seconds(uint32_t frames, const CynthSampleSpec* ss)
{
    return (double)frames / ss->rate;
}

double
cynth_envelope_get_volume(const CynthEnvelope* envelope,
                          double time_since_note_start,
                          double time_since_note_end)
{
    double env = 0;
    double duration = time_since_note_start - time_since_note_end;
    if (duration < envelope->attack) {
        env = (duration / envelope->attack);
    } else if (duration < envelope->decay + envelope->attack) {
        double t = (duration - envelope->attack) / envelope->decay;
        env = 1.0 + t * (envelope->sustain - 1.0);
    } else {
        env = envelope->sustain;
    }

    if (time_since_note_end > 0) {
        env *= 1 - (time_since_note_end / envelope->release);
    }

    if (env < 0)
        env = 0;

    return env;
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
cynth_buffer_write_notes(const CynthBuffer* buffer,
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
cynth_buffer_clear(const CynthBuffer* buffer)
{
    memset(buffer->data,
           0,
           sizeof(int16_t) * buffer->frame_amount * buffer->ss.channels);
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

double
cynth_oscillator_next_sample(CynthOscillator* oscillator,
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
cynth_voice_add_to_buffer(CynthVoice* voice,
                          CynthBuffer* buffer,
                          uint16_t start_frame,
                          uint16_t frame_amount)
{
    assert(start_frame < frame_amount);

    size_t f = start_frame;
    for (; f < frame_amount; f++) {
        double frequency = midi_to_freq(voice->current_note);
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

        double volume = 0.05;
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
cynth_voice_write_to_buffer(CynthVoice* voice,
                            CynthBuffer* buffer,
                            uint16_t start_frame,
                            uint16_t frame_amount)
{
    assert(start_frame < frame_amount);

    size_t f = start_frame;
    for (; f < frame_amount; f++) {
        double frequency = midi_to_freq(voice->current_note);
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
cynth_voice_note_start(CynthVoice* voice, uint8_t note)
{
    voice->is_note_on = true;
    voice->current_note = note;
    voice->frames_since_note_start = 0;
    voice->frames_since_note_end = 0;
}

void
cynth_voice_note_end(CynthVoice* voice)
{
    voice->is_note_on = false;
    voice->frames_since_note_end = 0;
}

void
cynth_voice_init(CynthVoice* voice,
                 float (*wave_fn)(float),
                 const CynthEnvelope* envelope)
{
    voice->oscillator = (CynthOscillator){ (rand() % 100) / 100.0, wave_fn };
    voice->envelope = envelope;
}

void
cynth_synthesizer_init(CynthSynthesizer* s,
                       float (*wave_fn)(float),
                       const CynthEnvelope* envelope)
{
    s->wave_fn = wave_fn;
    s->voice_amount = 0;
    s->envelope = envelope;
}

void
cynth_synthesizer_play_midi_events(CynthSynthesizer* s,
                                   CynthEngine* engine,
                                   CynthSampleSpec ss,
                                   CynthMIDIHeader header,
                                   CynthMIDIEvent* events,
                                   size_t event_amount)
{
    CynthBuffer buffer = { 0 };
    float buffer_time = 0.5f;
    int16_t* backing_data =
      calloc(buffer_time * ss.rate * ss.channels, sizeof(int16_t));
    cynth_buffer_init(&buffer, ss, buffer_time, backing_data);

    const uint16_t period = 1024;

    double last_evt_start = 0;
    size_t p = 0, e = 0;

    ClogProgressBar prog = { 0 };
    clog_progress_init(&prog, event_amount, 48);

    for (; e < event_amount; p++) {
        double period_start_time = cynth_frames_to_seconds(p * period, &ss);
        double period_end_time = cynth_frames_to_seconds((p + 1) * period, &ss);

        for (; e < event_amount; e++) {
            CynthMIDIEvent evt = events[e];
            double curr_evt_start = last_evt_start + cynth_midi_time_to_seconds(
                                                       header, evt.delta_time);

            if (curr_evt_start > period_end_time)
                break;

            if (evt.type == CYNTH_MIDI_EVENT_NOTE_ON) {
                cynth_synthesizer_note_start(s, s->envelope, evt.note.key);
            } else if (evt.type == CYNTH_MIDI_EVENT_NOTE_OFF) {
                cynth_synthesizer_note_end(s, evt.note.key);
            }

            last_evt_start = curr_evt_start;
            clog_progress_increment(&prog, 1);
        }

        cynth_buffer_clear(&buffer);
        cynth_synthesizer_write_to_buffer(s, &buffer, 0, period);
        cynth_engine_write_buffer(engine, &buffer, period);
    }
    clog_progress_finish(&prog);

    free(backing_data);
}

void
cynth_synthesizer_note_start(CynthSynthesizer* s,
                             const CynthEnvelope* envelope,
                             uint8_t note)
{
    if (s->voice_amount >= CYNTH_SYNTHESIZER_MAX_VOICE) {
        CLOG_WARNING("Synthesizer max. voice capacity reached.");
        return;
    }

    CynthVoice v = { 0 };
    cynth_voice_init(&v, s->wave_fn, envelope);
    cynth_voice_note_start(&v, note);
    s->voices[s->voice_amount++] = v;
    CLOG_DEBUG("[+]New voice amount: %d", s->voice_amount);
}

void
cynth_synthesizer_note_end(CynthSynthesizer* s, uint8_t note)
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
cynth_synthesizer_write_to_buffer(CynthSynthesizer* s,
                                  CynthBuffer* buffer,
                                  uint16_t start_frame,
                                  uint16_t frame_amount)
{
    /* Loop must be reversed so we can safely
     * remove elements during the loop. */
    int v = 0;
    for (v = s->voice_amount - 1; v >= 0; v--) {
        CynthVoice* voice = &s->voices[v];
        cynth_voice_add_to_buffer(voice, buffer, start_frame, frame_amount);

        bool release_ended = voice->frames_since_note_end >
                             voice->envelope->release * buffer->ss.rate;
        if (!release_ended)
            continue;

        /* Remove voice */
        if (v != s->voice_amount - 1) {
            CynthVoice temp = s->voices[v];
            s->voices[v] = s->voices[s->voice_amount - 1];
            s->voices[s->voice_amount - 1] = temp;
        }
        s->voice_amount--;
        CLOG_DEBUG("[-]New voice amount: %d", s->voice_amount);
    }
}
