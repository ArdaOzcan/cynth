#include <pulse/simple.h>
#include "ccore.h"
#include "cynth.h"

float
wave_custom(float t)
{
    return 0.51552f * caudio_triangle_normalized(t) +
           0.25776f * caudio_sine_normalized(t * 2.0f/1.0f);
           0.12888f * caudio_sine_normalized(t * 3.0f/2.0f);
           0.06444f * caudio_sine_normalized(t * 4.0f/3.0f);
           0.03222f * caudio_sine_normalized(t * 5.0f/4.0f);
}

int
main(void)
{
    pa_simple* s;
    pa_sample_spec ss;

    ss.format = PA_SAMPLE_S16NE;
    ss.channels = 2;
    ss.rate = 44100;

    s = pa_simple_new(NULL,         // Use the default server.
                      "CAudioDemo", // Our application's name.
                      PA_STREAM_PLAYBACK,
                      NULL,    // Use the default device.
                      "Music", // Description of our stream.
                      &ss,     // Our sample format.
                      NULL,    // Use default channel map
                      NULL,    // Use default buffering attributes.
                      NULL);   // Ignore error code.

    VArena varena = { 0 };
    varena_init(&varena, 1 << 22);
    Allocator alloc = varena_allocator(&varena);

    CAudioBuffer buffer = { 0 };
    caudio_buffer_init(&buffer, ss, 5.0f, &alloc);

    CAudioNote notes[] = {
        { CAUDIO_FREQ_C, 0.f, 0.5f, 0.1f },
        { CAUDIO_FREQ_G, 0.f, 1.5f, 0.1f },
        { CAUDIO_FREQ_E, 0.5f, 0.5f, 0.1f },
        { CAUDIO_FREQ_G, 1.f, 0.5f, 0.1f },
        { CAUDIO_FREQ_Fs, 1.5f, 0.5f, 0.1f },
        { CAUDIO_FREQ_B, 1.5f, 1.5f, 0.1f },
        { CAUDIO_FREQ_A, 2.f, 0.5f, 0.1f },
        { CAUDIO_FREQ_B, 2.5f, 0.5f, 0.1f },
        { CAUDIO_FREQ_B, 3.f, 1.5f, 0.1f },
        { CAUDIO_FREQ_D, 3.f, 1.5f, 0.1f },
        { CAUDIO_FREQ_G, 3.f, 1.5f, 0.1f },
    };

    CAudioNote* fifth_notes = array(CAudioNote, 16, &alloc);
    // float* third_notes = array(float, 16, &alloc);
    size_t i = 0;
    for (i = 0; i < sizeof(notes) / sizeof(*notes); i++) {
        array_append(fifth_notes, notes[i]);
        fifth_notes[array_len(fifth_notes) - 1].frequency =
          notes[i].frequency * 1.5f;

        // array_append(third_notes, notes[i] * 1.2599f);
    }

    // caudio_write_frequencies_sine(
    //   &buffer, notes, 8, 0.5f * ss.rate, 2.0f * ss.rate, 0.35f);
    // caudio_write_frequencies_square(
    //   &buffer, third_notes, 8, 0.5f * ss.rate, 2.0f * ss.rate, 0.25f);
    // caudio_write_frequencies_sine(
    //   &buffer, fifth_notes, 8, 0.5f * ss.rate, 2.0f * ss.rate, 0.35f);
    //

    caudio_write_notes(
      &buffer, notes, sizeof(notes) / sizeof(*notes), wave_custom);
    // caudio_write_notes(
    //   &buffer, fifth_notes, array_len(fifth_notes), caudio_sine_normalized);
    caudio_buffer_fprint(&buffer, stdout);
    int export_res = caudio_buffer_export_wav(&buffer, "out/out.wav");
    if (export_res != 0) {
        fprintf(stderr, "File out.wav could not be opened.");
        pa_simple_free(s);
        return 1;
    }

    size_t loop = 1;
    int error = 0;
    size_t pcm_data_size =
      buffer.sample_amount * buffer.ss.channels * sizeof(int16_t);
    for (; loop > 0; --loop) {
        if (pa_simple_write(s, buffer.data, pcm_data_size, &error) < 0) {
            fprintf(
              stderr, "pa_simple_write() failed: %s\n", pa_strerror(error));
            pa_simple_free(s);
            return 1;
        }
    }

    if (pa_simple_drain(s, &error) < 0) {
        fprintf(stderr, "pa_simple_drain() failed: %s\n", pa_strerror(error));
        pa_simple_free(s);
        return 1;
    }

    pa_simple_free(s);
}
