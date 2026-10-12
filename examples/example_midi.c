/* MIDI-to-WAV example (plus optional live playback).
 *
 * Build (from project root):
 *   cmake -S . -B build && cmake --build build
 *
 * Run (from project root):
 *   ./build/cynth_example_midi songs/c_major_scale.mid [track] [out.wav]
 * [--play]
 *   ./build/cynth_example_midi --help
 *
 * With no arguments it looks for songs/c_major_scale.mid next to the
 * project (also works from build/ via ../songs/...).
 * Without --play it only writes the WAV, so it works headless.
 */

#include "ccore.h"
#include "clog.h"
#include "cynth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
print_usage(const char* prog)
{
    fprintf(stderr, "Usage: %s [song.mid] [track] [out.wav] [--play]\n", prog);
}

static unsigned char*
read_file(const char* path, size_t* out_size)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    unsigned char* data = malloc((size_t)size);
    if (data == NULL) {
        fclose(f);
        return NULL;
    }
    if (size > 0 && fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_size = (size_t)size;
    return data;
}

/* A mellow organ-ish timbre: fundamental + a few harmonics. */
static float
wave_organ(float t)
{
    return 1.00f * cynth_triangle_normalized(t) +
           0.30f * cynth_triangle_normalized(t * 2.0f) +
           0.15f * cynth_triangle_normalized(t * 3.0f) +
           0.08f * cynth_triangle_normalized(t * 4.0f);
}

static const char*
find_default_midi(void)
{
    static const char* candidates[] = {
        "songs/c_major_scale.mid",
        "../songs/c_major_scale.mid",
        "c_major_scale.mid",
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        FILE* f = fopen(candidates[i], "rb");
        if (f != NULL) {
            fclose(f);
            return candidates[i];
        }
    }
    return NULL;
}

static size_t
count_note_ons(CynthMIDIEvent* events, size_t amount)
{
    size_t n = 0;
    for (size_t i = 0; i < amount; i++) {
        if (events[i].type == CYNTH_MIDI_EVENT_NOTE_ON) {
            n++;
        }
    }
    return n;
}

int
main(int argc, char** argv)
{
    clog_log_level_set(CLOG_LOG_LEVEL_WARNING);

    const char* midi_path = NULL;
    const char* out_path  = "midi_out.wav";
    long track_arg        = -1;
    int play              = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--play") == 0) {
            play = 1;
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (midi_path == NULL) {
            midi_path = argv[i];
        } else if (track_arg < 0) {
            char* end = NULL;
            long v    = strtol(argv[i], &end, 10);
            if (end != NULL && *end == '\0' && v >= 0) {
                track_arg = v;
            } else {
                out_path = argv[i];
            }
        } else {
            out_path = argv[i];
        }
    }

    if (midi_path == NULL) {
        midi_path = find_default_midi();
        if (midi_path == NULL) {
            fprintf(stderr, "No MIDI file given and no default found.\n");
            print_usage(argv[0]);
            return 1;
        }
        printf("Using default MIDI file: %s\n", midi_path);
    }

    size_t file_size         = 0;
    unsigned char* file_data = read_file(midi_path, &file_size);
    if (file_data == NULL) {
        fprintf(stderr, "Could not read '%s'.\n", midi_path);
        return 1;
    }

    CynthMIDIObject midi = { 0 };
    cynth_midi_import(file_data, file_size, &midi);
    printf("MIDI: format=%d tracks=%u division=%u tempo=%u\n",
           (int)midi.header.format,
           midi.header.num_tracks,
           midi.header.division,
           midi.header.tempo);

    size_t track_count = array_len(midi.tracks);
    if (track_count == 0) {
        fprintf(stderr, "No tracks found in '%s'.\n", midi_path);
        free(file_data);
        return 1;
    }
    for (size_t t = 0; t < track_count; t++) {
        printf(
          "  track %zu: %zu events (%zu note-ons)\n",
          t,
          midi.tracks[t].event_amount,
          count_note_ons(midi.tracks[t].events, midi.tracks[t].event_amount));
    }

    size_t track_idx = 0;
    if (track_arg >= 0) {
        if ((size_t)track_arg >= track_count) {
            fprintf(stderr,
                    "Track %ld out of range (0..%zu).\n",
                    track_arg,
                    track_count - 1);
            free(file_data);
            return 1;
        }
        track_idx = (size_t)track_arg;
    } else {
        /* Default: track with the most note-on events. */
        size_t best = 0;
        for (size_t t = 0; t < track_count; t++) {
            if (count_note_ons(midi.tracks[t].events,
                               midi.tracks[t].event_amount) > best) {
                best      = count_note_ons(midi.tracks[t].events,
                                           midi.tracks[t].event_amount);
                track_idx = t;
            }
        }
    }

    CynthMIDITrackInfo track = midi.tracks[track_idx];
    CynthNote* notes = calloc(track.event_amount / 2 + 1, sizeof(CynthNote));
    if (notes == NULL) {
        fprintf(stderr, "Out of memory.\n");
        free(file_data);
        return 1;
    }
    size_t note_amount = 0;
    cynth_midi_track_to_notes(track, midi.header, notes, &note_amount);
    printf("Track %zu: %zu notes.\n", track_idx, note_amount);
    if (note_amount == 0) {
        fprintf(stderr, "Selected track contains no notes.\n");
        free(notes);
        free(file_data);
        return 1;
    }

    CynthEnvelope env = {
        .attack = 0.01f, .decay = 0.15f, .sustain = 0.6f, .release = 0.3f
    };
    CynthSampleSpec ss = { .format   = CYNTH_SAMPLE_S16LE,
                           .rate     = 44100,
                           .channels = 2 };

    float song_end = 0.0f;
    for (size_t i = 0; i < note_amount; i++) {
        float end = notes[i].start_time + notes[i].duration;
        if (end > song_end) {
            song_end = end;
        }
    }
    uint32_t frames =
      (uint32_t)((song_end + env.release + 1.0f) * (float)ss.rate) + 1;
    int16_t* pcm = calloc((size_t)frames * ss.channels, sizeof(int16_t));
    if (pcm == NULL) {
        fprintf(stderr, "Out of memory.\n");
        free(notes);
        free(file_data);
        return 1;
    }

    CynthBuffer buffer = { 0 };
    cynth_buffer_init(&buffer, ss, frames, pcm);
    cynth_buffer_write_notes(&buffer, notes, note_amount, env, wave_organ);

    if (cynth_buffer_export_wav(&buffer, out_path) != 0) {
        fprintf(stderr, "Could not write '%s'.\n", out_path);
        free(pcm);
        free(notes);
        free(file_data);
        return 1;
    }
    printf("Wrote %s (%.2f s, %u frames).\n", out_path, song_end, frames);

    if (play) {
        CynthSampleSpec play_ss = ss;
        CynthEngine* engine     = cynth_engine_init(&play_ss, "default");
        if (engine == NULL) {
            fprintf(stderr,
                    "Audio device unavailable, skipping playback "
                    "(WAV above is still valid).\n");
        } else {
            if (cynth_engine_write_buffer(
                  engine, &buffer, buffer.frame_amount) != CYNTH_ERROR_NONE) {
                fprintf(stderr, "Playback write failed.\n");
            }
            cynth_engine_drain(engine);
            cynth_engine_deinit(engine);
            printf("Playback finished.\n");
        }
    } else {
        printf("Hint: pass --play to also hear it live.\n");
    }

    free(pcm);
    free(notes);
    free(file_data);
    return 0;
}
