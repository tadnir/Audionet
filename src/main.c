#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "audio/audio.h"
#include "logger.h"
#include "fft/fft.h"


int compare_floats(const void* a, const void* b) {
    // Note: Reverse comparison for descending order
    if ((*(float*)b - *(float*)a) > 0) {
        return 1;
    } else if ((*(float*)b - *(float*)a) < 0) {
        return -1;
    }

    return 0;
}

int compare_magnitudes(const struct frequency_and_magnitude* a, const struct frequency_and_magnitude* b) {
    return compare_floats(&a->magnitude, &b->magnitude);
}

void fft_recording(fft_t* fft, const float* recorded_frame, size_t size) {
    int ret;
    const struct frequency_and_magnitude* frequencies;
    size_t count_frequencies;

    ret = FFT__calculate(fft, recorded_frame, size, &frequencies, &count_frequencies);
    if (ret != 0) {
        LOG_ERROR("Failed to calculate fft on provided sound frame");
        return;
    }

    /* Sort the frequencies by their magnitudes */
    qsort((void*)frequencies, count_frequencies, sizeof(struct frequency_and_magnitude),
          (int (*)(const void *, const void *)) compare_magnitudes);

    /* Print the most significant frequencies */
#define MAX_FREQUENCIES 3
    if (frequencies[0].magnitude > 0.1) {
        for (int i = 0; i < MAX_FREQUENCIES; i++) {
            if (frequencies[i].magnitude < 0.1) {
                break;
            }
            printf("%.1fHz(%.2f), ", 50.0 * floor((frequencies[i].frequency/50.0)+0.5), frequencies[i].magnitude);
        }
        printf("\n");
    }

    free((void*)frequencies);
}


int main(int argc, char **argv) {
    int status = -1;
    fft_t* fft = NULL;
    audio_t* audio = NULL;

    LOG_INFO("Started..");
    fft = FFT__initialize(3600, SAMPLE_RATE_48000);
    if (fft == NULL) {
        LOG_ERROR("Failed to initialize fft");
        status = -1;
        goto l_cleanup;
    }

    audio = AUDIO__initialize(SAMPLE_RATE_48000);
    if (audio == NULL) {
        LOG_ERROR("Failed to initialize audio");
        status = -1;
        goto l_cleanup;
    }

    struct frequency_output freqs[] = {
            { .amplitude = 1, .frequency = 400},
            { .amplitude = 1, .frequency = 450},
            { .amplitude = 1, .frequency = 500},
    };
    status = AUDIO__set_playing_frequencies(audio, freqs, sizeof(freqs)/sizeof(freqs[0]));
    if (status != 0) {
        LOG_ERROR("Failed to set frequencies");
        goto l_cleanup;
    }

    AUDIO__set_recording_callback(audio, (recording_callback_t) fft_recording, fft);
    status = AUDIO__start(audio);
    if (status != 0) {
        LOG_ERROR("Failed to start audio");
        goto l_cleanup;
    }

    printf("Press Enter to quit...\n");
    printf("Got char %d", getchar());

    LOG_INFO("Finished");
    status = 0;
l_cleanup:
    if (audio != NULL) {
        AUDIO__free(audio);
        audio = NULL;
    }

    if (fft != NULL) {
        FFT__free(fft);
        fft = NULL;
    }

    return status;
}