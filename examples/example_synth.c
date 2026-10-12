/* Offline synthesis example: render a C-major scale + final chord to WAV.
 *
 * Build (from project root):
 *   cmake -S . -B build && cmake --build build
 *
 * Run (from project root):
 *   ./build/cynth_example_synth [out.wav] [--wave sine|triangle|square]
 * [--play]
 *
 * This example needs no audio device: it always writes a WAV file.
 * Live playback is opt-in via --play and fails gracefully headless.
 */

#include "cynth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
print_usage(const char* prog)
{
    fprintf(stderr,
            "Usage: %s [out.wav] [--wave sine|triangle|square] [--play]\n",
            prog);
}

static float (*parse_wave(const char* name))(float)
{
    if (name == NULL) {
        return cynth_sine_normalized;
    }
    if (strcmp(name, "sine") == 0) {
        return cynth_sine_normalized;
    }
    if (strcmp(name, "triangle") == 0) {
        return cynth_triangle_normalized;
    }
    if (strcmp(name, "square") == 0) {
        return cynth_square_normalized;
    }
    return NULL;
}

int
main(int argc, char** argv)
{
    const char* out_path  = "scale.wav";
    const char* wave_name = "sine";
    int play              = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--play") == 0) {
            play = 1;
        } else if (strcmp(argv[i], "--wave") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            wave_name = argv[i];
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            out_path = argv[i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    float (*wave_fn)(float) = parse_wave(wave_name);
    if (wave_fn == NULL) {
        fprintf(stderr, "Unknown wave '%s'.\n", wave_name);
        print_usage(argv[0]);
        return 1;
    }

    /* C major scale: C4..C5, then a C-major triad held together. */
    const int scale_midi[] = { 60, 62, 64, 65, 67, 69, 71, 72 };
    const size_t scale_len = sizeof(scale_midi) / sizeof(scale_midi[0]);
    const float step       = 0.45f;
    const float tone_len   = 0.40f;

    /* 8 scale tones + 3 chord tones. */
    CynthNote notes[11];
    for (size_t i = 0; i < scale_len; i++) {
        notes[i].frequency  = (float)cynth_midi_to_freq(scale_midi[i]);
        notes[i].start_time = (float)i * step;
        notes[i].duration   = tone_len;
        notes[i].volume     = 0.5f;
    }
    const float chord_start = (float)scale_len * step + 0.2f;
    const int chord_midi[]  = { 60, 64, 67 };
    for (size_t i = 0; i < 3; i++) {
        notes[scale_len + i].frequency =
          (float)cynth_midi_to_freq(chord_midi[i]);
        notes[scale_len + i].start_time = chord_start;
        notes[scale_len + i].duration   = 1.5f;
        notes[scale_len + i].volume     = 0.25f;
    }
    const size_t note_amount = scale_len + 3;

    CynthEnvelope env = {
        .attack = 0.01f, .decay = 0.1f, .sustain = 0.7f, .release = 0.3f
    };

    CynthSampleSpec ss = { .format   = CYNTH_SAMPLE_S16LE,
                           .rate     = 44100,
                           .channels = 2 };

    const float tail      = env.release + 0.5f;
    const float total     = chord_start + 1.5f + tail;
    const uint32_t frames = (uint32_t)(total * (float)ss.rate) + 1;

    int16_t* data = calloc((size_t)frames * ss.channels, sizeof(int16_t));
    if (data == NULL) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    CynthBuffer buffer = { 0 };
    cynth_buffer_init(&buffer, ss, frames, data);
    cynth_buffer_write_notes(&buffer, notes, note_amount, env, wave_fn);

    if (cynth_buffer_export_wav(&buffer, out_path) != 0) {
        fprintf(stderr, "Could not write '%s'.\n", out_path);
        free(data);
        return 1;
    }
    printf("Wrote %s (%.2f s, %u frames, %s wave).\n",
           out_path,
           total,
           frames,
           wave_name);

    if (play) {
        CynthSampleSpec play_ss = ss;
        CynthEngine* engine     = cynth_engine_init(&play_ss, "default");
        if (engine == NULL) {
            fprintf(stderr,
                    "Audio device unavailable, skipping playback "
                    "(WAV above is still valid).\n");
        } else {
            CynthError err =
              cynth_engine_write_buffer(engine, &buffer, buffer.frame_amount);
            if (err != CYNTH_ERROR_NONE) {
                fprintf(stderr, "Playback write failed.\n");
            }
            cynth_engine_drain(engine);
            cynth_engine_deinit(engine);
            printf("Playback finished.\n");
        }
    }

    free(data);
    return 0;
}
