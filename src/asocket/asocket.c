#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include "asocket.h"

#include "audio/audio.h"
#include "logger.h"
#include "fft/fft.h"

#define BASE_CHANNEL_FREQUENCY (200)
#define CHANNEL_FREQUENCY_BAND_WIDTH (50)
#define NUMBER_OF_CHANNELS (13)
#define NUMBER_OF_CONCURRENT_CHANNELS (3)
#define AMPLITUDE_MAGNITUDE_THRESHOLD (0.1)

#define FREQUENCY_TO_CHANNEL_INDEX(frequency) (                                                           \
    (ROUND_TO((frequency), CHANNEL_FREQUENCY_BAND_WIDTH) / CHANNEL_FREQUENCY_BAND_WIDTH)                  \
    - (ROUND_TO(BASE_CHANNEL_FREQUENCY, CHANNEL_FREQUENCY_BAND_WIDTH) / CHANNEL_FREQUENCY_BAND_WIDTH)     \
)
#define ROUND_TO(number, rounder) (((float)rounder) * floor((((float)number)/((float)rounder))+0.5))

struct asocket_s {
    audio_t* audio;
    fft_t* fft;
};

uint64_t comb(int n, int r) {
    uint64_t sum = 1;
    // Calculate the value of n choose r using the binomial coefficient formula
    for(int i = 1; i <= r; i++) {
        sum = sum * (n - r + i) / i;
    }

    return sum;
}

static uint64_t channels_to_int(int a, int b, int c) {
//    def channels_to_int(a, b, c):
//        b=b-1-a
//        c=c-2-a
//        nb = sum(range(11-a-b, 11-a, 1))
//        na = sum([nCr(i+1, 2) for i in range(12-a, 12, 1)])
//        return na + nb + c
    b = b - a - 1;
    c = c - a - 2;

    uint64_t na = 0;
    for (int i = 12-a; i < 12; ++i) {
        na += comb(i+1, 2);
    }

    int nb = 0;
    for (int i = 11 - a - b; i < 11 - a; ++i) {
        nb += i;
    }

    return na + nb + c;
}

static void int_to_channels(uint64_t value, int channels[3]) {
    int a;
    for (a = NUMBER_OF_CHANNELS - 2; a >= 0; a--) {
        uint64_t na = channels_to_int(a, a+1, a+2);
        if (value >= na) {
            break;
        }
    }

    int b;
    for (b = NUMBER_OF_CHANNELS - 1; b > a; b--) {
        uint64_t nb = channels_to_int(a, b, b+1);
        if (value >= nb) {
            break;
        }
    }

    int c;
    for (c = NUMBER_OF_CHANNELS; c > b; c--) {
        uint64_t nc = channels_to_int(a, b, c);
        if (value == nc) {
            break;
        }
    }

    channels[0] = a;
    channels[1] = b;
    channels[2] = c;
}

static int compare_ints(const void *va, const void *vb)
{
    int a = *(int *)va, b = *(int *) vb;
    return a < b ? -1 : a > b ? +1 : 0;
}

static int reverse_compare_floats(const void* a, const void* b) {
    // Note: Reverse comparison for descending order
    if ((*(float*)b - *(float*)a) > 0) {
        return 1;
    } else if ((*(float*)b - *(float*)a) < 0) {
        return -1;
    }

    return 0;
}

static int compare_magnitudes(const struct frequency_and_magnitude* a, const struct frequency_and_magnitude* b) {
    return reverse_compare_floats(&a->magnitude, &b->magnitude);
}

static void listen_callback(asocket_t* socket, const float* recorded_frame, size_t size) {
    int ret;
    const struct frequency_and_magnitude* frequencies = NULL;
    size_t count_frequencies;

    ret = FFT__calculate(socket->fft, recorded_frame, size, &frequencies, &count_frequencies);
    if (ret != 0) {
        LOG_ERROR("Failed to calculate fft on provided sound frame");
        return;
    }

    if (count_frequencies < NUMBER_OF_CONCURRENT_CHANNELS) {
        LOG_ERROR("Expected at least %d frequencies output from fft, got: %lld", NUMBER_OF_CONCURRENT_CHANNELS, count_frequencies);
        goto l_cleanup;
    }

    /* Sort the frequencies by their magnitudes */
    qsort((void*)frequencies, count_frequencies, sizeof(struct frequency_and_magnitude),
          (int (*)(const void *, const void *)) compare_magnitudes);

    /* If there aren't at least NUMBER_OF_CONCURRENT_CHANNELS frequencies with some noticeable sound, we consider it as quiet. */
    if (frequencies[NUMBER_OF_CONCURRENT_CHANNELS - 1].magnitude <= AMPLITUDE_MAGNITUDE_THRESHOLD) {
        LOG_INFO("Quiet");
        goto l_cleanup;
    }

    int values[NUMBER_OF_CONCURRENT_CHANNELS];
    for (int i = 0; i < NUMBER_OF_CONCURRENT_CHANNELS; i++) {
        int frequency_value = FREQUENCY_TO_CHANNEL_INDEX(frequencies[i].frequency);
        printf("%d (%.0fHz),  ", frequency_value, frequencies[i].frequency);
        values[i] = frequency_value;
    }
    printf("\n");

    qsort(values, NUMBER_OF_CONCURRENT_CHANNELS, sizeof(int), compare_ints);
    if (values[0] == values[1] || values[1] == values[2]) {
        LOG_ERROR("Got two frequencies on the same channel");
        goto l_cleanup;
    }

    LOG_INFO("values: %d, %d, %d -> %lld", values[0], values[1], values[2], channels_to_int(values[0], values[1], values[2]));

l_cleanup:
    if (frequencies != NULL) {
        free((void*) frequencies);
        frequencies = NULL;
    }
}

asocket_t* ASOCKET__initialize() {
    asocket_t* socket = malloc(sizeof(asocket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to initialize Audio Socket");
        return NULL;
    }

    LOG_DEBUG("Initializing FFT");
    socket->fft = FFT__initialize(3600, SAMPLE_RATE_48000);
    if (socket->fft == NULL) {
        LOG_ERROR("Failed to initialize fft");
        free(socket);
        return NULL;
    }

    LOG_DEBUG("Initializing Audio");
    socket->audio = AUDIO__initialize(SAMPLE_RATE_48000);
    if (socket->audio == NULL) {
        LOG_ERROR("Failed to initialize audio");
        FFT__free(socket->fft);
        free(socket);
        return NULL;
    }

    LOG_DEBUG("Starting Audio");
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

//    union {
//        int values[3];
//        struct {
//            int a;
//            int b;
//            int c;
//        } channels;
//    } p;
//    for (int i = 0; i < 285; ++i) {
//        int_to_channels(i, p.values);
//        uint64_t value = channels_to_int(p.channels.a, p.channels.b, p.channels.c);
//        LOG_INFO("%lld -> %d, %d, %d -> %lld", i, p.channels.a, p.channels.b, p.channels.c, value);
//        assert(i == value);
//    }

    struct frequency_output freqs[] = {
            { .amplitude = 1, .frequency = 200},
            { .amplitude = 1, .frequency = 250},
            { .amplitude = 1, .frequency = 300},
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

