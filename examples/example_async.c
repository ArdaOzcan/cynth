#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PCM_DEVICE "default"
#define PI 3.14159265358979323846

int
main(void)
{
    snd_pcm_t* pcm_handle;
    snd_pcm_hw_params_t* params;
    snd_pcm_uframes_t frames;
    int retval;

    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    unsigned int rate       = 44100;
    int channels            = 2;

    int seconds = 2;
    double freq = 440.0;

    if ((retval = snd_pcm_open(
           &pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr,
                "ERROR: Can't open \"%s\" PCM device. %s\n",
                PCM_DEVICE,
                snd_strerror(retval));
        return -1;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm_handle, params);

    snd_pcm_hw_params_set_access(
      pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, format);
    snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);

    if ((retval = snd_pcm_hw_params(pcm_handle, params)) < 0) {
        fprintf(stderr,
                "ERROR: Can't set hardware params. %s\n",
                snd_strerror(retval));
        return -1;
    }

    snd_pcm_hw_params_free(params);

    snd_pcm_prepare(pcm_handle);

    snd_pcm_hw_params_get_period_size(params, &frames, 0);

    int16_t* buffer;
    int buffer_size = frames * channels;
    buffer          = (int16_t*)malloc(buffer_size * sizeof(int16_t));

    int num_samples = seconds * rate;
    for (int i = 0; i < num_samples; i += frames) {
        for (int f = 0; f < frames; f++) {
            double t          = (double)(i + f) / rate;
            double sample     = sin(2.0 * PI * freq * t);
            buffer[2 * f]     = (int16_t)(sample * INT16_MAX);
            buffer[2 * f + 1] = (int16_t)(sample * INT16_MAX);
        }
        retval = snd_pcm_writei(pcm_handle, buffer, frames);
        if (retval == -EPIPE) {
            // Buffer underrun
            snd_pcm_prepare(pcm_handle);
        } else if (retval < 0) {
            fprintf(stderr,
                    "ERROR: Can't write to PCM device. %s\n",
                    snd_strerror(retval));
        }
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    free(buffer);

    return 0;
}
