#include <malloc.h>
#include <math.h>
#include "asocket.h"

#include "audio/audio.h"
#include "logger.h"
#include "fft/fft.h"

struct asocket_s {
    audio_t* audio;
    fft_t* fft;
};

static int compare_floats(const void* a, const void* b) {
    // Note: Reverse comparison for descending order
    if ((*(float*)b - *(float*)a) > 0) {
        return 1;
    } else if ((*(float*)b - *(float*)a) < 0) {
        return -1;
    }

    return 0;
}

static int compare_magnitudes(const struct frequency_and_magnitude* a, const struct frequency_and_magnitude* b) {
    return compare_floats(&a->magnitude, &b->magnitude);
}

static void listen_callback(asocket_t* socket, const float* recorded_frame, size_t size) {
    int ret;
    const struct frequency_and_magnitude* frequencies;
    size_t count_frequencies;

    ret = FFT__calculate(socket->fft, recorded_frame, size, &frequencies, &count_frequencies);
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

asocket_t* ASOCKET__initialize() {
    asocket_t* socket = malloc(sizeof(asocket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to initialize Audio Socket");
        return NULL;
    }

    LOG_INFO("Started..");
    socket->fft = FFT__initialize(3600, SAMPLE_RATE_48000);
    if (socket->fft == NULL) {
        LOG_ERROR("Failed to initialize fft");
        free(socket);
        return NULL;
    }

    socket->audio = AUDIO__initialize(SAMPLE_RATE_48000);
    if (socket->audio == NULL) {
        LOG_ERROR("Failed to initialize audio");
        FFT__free(socket->fft);
        free(socket);
        return NULL;
    }

    int status = AUDIO__start(socket->audio);
    if (status != 0) {
        LOG_ERROR("Failed to start audio");
        ASOCKET__free(socket);
        return NULL;
    }

    return socket;
}

void ASOCKET__free(asocket_t* socket) {
    if (socket->audio != NULL) {
        (void)AUDIO__stop(socket->audio);
        AUDIO__free(socket->audio);
        socket->audio = NULL;
    }

    if (socket->fft != NULL) {
        FFT__free(socket->fft);
        socket->fft = NULL;
    }

    free(socket);
}

int ASOCKET__listen(asocket_t* socket) {
    AUDIO__set_recording_callback(socket->audio, (recording_callback_t) listen_callback, socket);
    return 0;
}

int ASOCKET__send(asocket_t* socket, void* data, size_t size) {
    int status = -1;

    struct frequency_output freqs[] = {
            { .amplitude = 1, .frequency = 400},
            { .amplitude = 1, .frequency = 450},
            { .amplitude = 1, .frequency = 500},
    };
    status = AUDIO__set_playing_frequencies(socket->audio, freqs, sizeof(freqs)/sizeof(freqs[0]));
    if (status != 0) {
        LOG_ERROR("Failed to set frequencies");
        return -1;
    }

    return 0;
}

ssize_t ASOCKET__recv(asocket_t* socket, void* data, size_t size) {
    return -1;
}

