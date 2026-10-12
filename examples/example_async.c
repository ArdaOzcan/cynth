/* Live-streaming example using the public Cynth engine API.
 *
 * Streams a two-note chord (A4 + E5) period by period, exactly like a
 * real-time player would, while also capturing the output to a WAV file.
 *
 * Build (from project root):
 *   cmake -S . -B build && cmake --build build
 *
 * Run:
 *   ./build/cynth_example_async [out.wav] [--play]
 *
 * Without --play it only renders offline (works headless). With --play it
 * opens the default ALSA device; if that fails it still writes the WAV.
 */

#include "cynth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
print_usage(const char* prog)
{
    fprintf(stderr, "Usage: %s [out.wav] [--play]\n", prog);
}

int
main(int argc, char** argv)
{
    const char* out_path = "live.wav";
    int play = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--play") == 0) {
            play = 1;
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

    CynthSampleSpec ss = { .format = CYNTH_SAMPLE_S16LE,
                           .rate = 44100,
                           .channels = 2 };

    CynthEngine* engine = NULL;
    uint32_t period = 1024;
    if (play) {
        CynthSampleSpec play_ss = ss;
        engine = cynth_engine_init(&play_ss, "default");
        if (engine == NULL) {
            fprintf(stderr,
                    "Audio device unavailable, rendering offline only.\n");
        } else {
            ss = play_ss; /* Keep any rate adjustment the backend made. */
            period = cynth_engine_get_period_size(engine);
            if (period == 0) {
                period = 1024;
            }
            printf("Streaming with period=%u rate=%u.\n",
                   period,
                   ss.rate);
        }
    }

    /* 2 s chord + 1 s release tail. */
    const float note_seconds = 2.0f;
    const float tail_seconds = 1.0f;
    const float total_seconds = note_seconds + tail_seconds;
    const uint32_t total_frames =
        (uint32_t)(total_seconds * (float)ss.rate);
    const uint32_t note_off_frame =
        (uint32_t)(note_seconds * (float)ss.rate);

    int16_t* period_data =
        calloc((size_t)period * ss.channels, sizeof(int16_t));
    int16_t* capture =
        calloc((size_t)total_frames * ss.channels, sizeof(int16_t));
    if (period_data == NULL || capture == NULL) {
        fprintf(stderr, "Out of memory.\n");
        free(period_data);
        free(capture);
        cynth_engine_deinit(engine);
        return 1;
    }

    CynthEnvelope env = { .attack = 0.01f,
                          .decay = 0.1f,
                          .sustain = 0.8f,
                          .release = 0.5f };
    CynthSynthesizer synth = { 0 };
    cynth_synthesizer_init(&synth, cynth_sine_normalized, 1.0f, env);
    cynth_synthesizer_note_start(&synth, 69); /* A4 */
    cynth_synthesizer_note_start(&synth, 76); /* E5 */

    CynthBuffer period_buf = { 0 };
    cynth_buffer_init(&period_buf, ss, period, period_data);

    uint32_t done = 0;
    int notes_off = 0;
    while (done < total_frames) {
        uint32_t chunk = period;
        if (chunk > total_frames - done) {
            chunk = total_frames - done;
        }

        if (!notes_off && done + chunk >= note_off_frame) {
            cynth_synthesizer_note_end(&synth, 69);
            cynth_synthesizer_note_end(&synth, 76);
            notes_off = 1;
        }

        cynth_buffer_clear(&period_buf);
        period_buf.frame_amount = chunk;
        cynth_synthesizer_write_to_buffer(&synth, &period_buf, 0, (uint16_t)chunk);

        memcpy(&capture[(size_t)done * ss.channels],
               period_data,
               (size_t)chunk * ss.channels * sizeof(int16_t));

        if (engine != NULL) {
            if (cynth_engine_write_buffer(engine, &period_buf, chunk) !=
                CYNTH_ERROR_NONE) {
                fprintf(stderr, "Playback write failed, continuing offline.\n");
                cynth_engine_deinit(engine);
                engine = NULL;
            }
        }

        done += chunk;
    }

    if (engine != NULL) {
        cynth_engine_drain(engine);
        cynth_engine_deinit(engine);
        printf("Playback finished.\n");
    }

    /* NOTE: cynth_buffer_init() zeroes its backing store, so it must only
     * be used on empty buffers. `capture` is already filled -- wrap it
     * directly instead. */
    CynthBuffer full = { .data = capture,
                         .frame_amount = total_frames,
                         .ss = ss };
    if (cynth_buffer_export_wav(&full, out_path) != 0) {
        fprintf(stderr, "Could not write '%s'.\n", out_path);
        free(period_data);
        free(capture);
        return 1;
    }
    printf("Wrote %s (%.2f s, %u frames).\n",
           out_path,
           total_seconds,
           total_frames);

    free(period_data);
    free(capture);
    return 0;
}
