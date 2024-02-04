#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "asocket.h"

#include "audio/audio.h"
#include "logger.h"
#include "fft/fft.h"

#define BASE_CHANNEL_FREQUENCY (100)
#define CHANNEL_FREQUENCY_BAND_WIDTH (150)
#define NUMBER_OF_CHANNELS (13)
#define NUMBER_OF_CONCURRENT_CHANNELS (3)
#define AMPLITUDE_MAGNITUDE_THRESHOLD (0.1)
#define SYMBOL_LENGTH_MILLISECONDS (150)

#define FREQUENCY_TO_CHANNEL_INDEX(frequency) (                                                           \
    (ROUND_TO((frequency), CHANNEL_FREQUENCY_BAND_WIDTH) / CHANNEL_FREQUENCY_BAND_WIDTH)                  \
    - (ROUND_TO(BASE_CHANNEL_FREQUENCY, CHANNEL_FREQUENCY_BAND_WIDTH) / CHANNEL_FREQUENCY_BAND_WIDTH)     \
)
#define CHANNEL_INDEX_TO_FREQUENCY(channel) (                                                             \
    (                                                                                                     \
        (channel)                                                                                         \
        + (ROUND_TO(BASE_CHANNEL_FREQUENCY, CHANNEL_FREQUENCY_BAND_WIDTH) / CHANNEL_FREQUENCY_BAND_WIDTH) \
    ) *  CHANNEL_FREQUENCY_BAND_WIDTH                                                                     \
)
#define ROUND_TO(number, rounder) (((float)rounder) * floor((((float)number)/((float)rounder))+0.5))

enum signals {

    /* 256 byte values 0-255 */

    SIGNAL_PREAMBLE = 270,
    SIGNAL_SEP = 275,
    SIGNAL_POST = 280,

    RANGE_END = 285 /* inclusive */
};

enum state_e {
    STATE_PREAMBLE,
    STATE_WORD,
    STATE_SEP,
};

struct asocket_s {
    audio_t* audio;
    fft_t* fft;
    enum state_e state;
    unsigned char votes[256];
    bool voted;
    char buffer[1024];
    int index;
};

uint64_t comb(int n, int r) {
    uint64_t sum = 1;
    // Calculate the value of n choose r using the binomial coefficient formula
    for(int i = 1; i <= r; i++) {
        sum = sum * (n - r + i) / i;
    }

    return sum;
}

static uint64_t channels_to_int(unsigned int a, unsigned int b, unsigned int c) {
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

struct channels {
    int a;
    int b;
    int c;
};

static struct channels int_to_channels(uint64_t value) {
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

    struct channels out = {a, b, c};
    LOG_INFO("out: %d, %d, %d -> %llu (%c)", a, b, c, value, value == SIGNAL_PREAMBLE ? '^': value == SIGNAL_SEP ? '+': value == SIGNAL_POST ? '$' : (char)value);
    return out;
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

    /* If there aren't at least NUMBER_OF_CONCURRENT_CHANNELS frequencies with some noticeable sound,
     * we consider it as quiet and there's no reason to continue. */
    if (frequencies[NUMBER_OF_CONCURRENT_CHANNELS - 1].magnitude <= AMPLITUDE_MAGNITUDE_THRESHOLD) {
        goto l_cleanup;
    }

    unsigned int channels[NUMBER_OF_CONCURRENT_CHANNELS];
    int channels_found = 0;
    for (int i = 0; i < count_frequencies; i++) {
        if (frequencies[i].magnitude <= AMPLITUDE_MAGNITUDE_THRESHOLD) {
            LOG_DEBUG("Got to frequency index %d and couldn't find channel strong sounds", i);
            break;
        }
        
        unsigned int channel = FREQUENCY_TO_CHANNEL_INDEX(frequencies[i].frequency);
        if (channel >= NUMBER_OF_CHANNELS) {
//            LOG_DEBUG("Invalid channel number: %d (probably due to noise)", channel);
            continue;
        }

        bool is_new = true;
        for (int j = 0; j < channels_found; ++j) {
            if (channel == channels[j]) {
//                LOG_DEBUG("Channel %d found twice, might be due to noise, multi-channel speaker or algo collision", channel);
                is_new = false;
            }
        }

        if (is_new) {
            printf("%d (%.0fHz),  ", channel, frequencies[i].frequency);
            channels[channels_found] = channel;
            channels_found++;

            if (channels_found == NUMBER_OF_CONCURRENT_CHANNELS) {
                break;
            }
        }

    }
//    printf("\n");

    /* Couldn't find enough channels */
    if (channels_found < NUMBER_OF_CONCURRENT_CHANNELS) {
        goto l_cleanup;
    }

    qsort(channels, NUMBER_OF_CONCURRENT_CHANNELS, sizeof(int), compare_ints);
    uint64_t value = channels_to_int(channels[0], channels[1], channels[2]);
    LOG_INFO("values: %d, %d, %d -> %lld", channels[0], channels[1], channels[2], value);
    switch (value) {
        case 0 ... 255:
            if (socket->state == STATE_WORD) {
                socket->voted = true;
                socket->votes[value]++;
                for (int i = 0; i < 256; ++i) {
                    if (socket->votes[i] != 0)
                        printf("%d,\t", i);
                }
                printf("\n");
                for (int i = 0; i < 256; ++i) {
                    if (socket->votes[i] != 0)
                        printf("%d,\t", socket->votes[i]);
                }
                printf("\n");
            }
            break;
        case SIGNAL_PREAMBLE ... SIGNAL_SEP-1:
            if (socket->state == STATE_PREAMBLE) {
                socket->index = 0;
                socket->state = STATE_WORD;
            }
            break;
        case SIGNAL_SEP ... SIGNAL_POST - 1:
            if (socket->state == STATE_WORD && socket->voted) {
                socket->voted = false;
                unsigned char max = 0;
                for (int i = 0; i < 256; ++i) {
                    if (socket->votes[i] > max) {
                        max = (unsigned char)i;
                    }
                }
                memset(socket->votes, 0, 256);
                socket->buffer[socket->index] = max;
                socket->index++;
                LOG_DEBUG("data: %hhx (%c)", max, max);
            }
            break;
        case SIGNAL_POST + 2 ... RANGE_END:
            if (socket->state != STATE_PREAMBLE) {
                //TODO: output the accumulated buffer.
                socket->buffer[socket->index] = '\0';
                printf("buffer: <%s>", socket->buffer);
//                memset(socket->buffer, 0, 1024);
//                socket->index = 0;
                socket->state = STATE_PREAMBLE;
            }
            break;
        default:
            LOG_DEBUG("Unknown signal %llu", value);
            break;
    }

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

    socket->state = STATE_PREAMBLE;
    socket->index = 0;
    memset(socket->votes, 0, 256);
    socket->voted = false;
    memset(socket->buffer, 0, 1024);

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
    struct channels channels;

    struct sound* sounds_packet = malloc((2 + 2 * size) * sizeof(struct sound));
    if (sounds_packet == NULL) {
        LOG_ERROR("Failed to allocate sounds packet");
        return -1;
    }

    channels = int_to_channels(SIGNAL_PREAMBLE+1);
    sounds_packet[0].length_milliseconds = SYMBOL_LENGTH_MILLISECONDS * 2;
    sounds_packet[0].number_of_frequencies = NUMBER_OF_CONCURRENT_CHANNELS;
    sounds_packet[0].frequencies[0] = CHANNEL_INDEX_TO_FREQUENCY(channels.a);
    sounds_packet[0].frequencies[1] = CHANNEL_INDEX_TO_FREQUENCY(channels.b);
    sounds_packet[0].frequencies[2] = CHANNEL_INDEX_TO_FREQUENCY(channels.c);

    for (int i = 0; i < size; i++) {
        channels = int_to_channels(((uint8_t*)data)[i]);
        sounds_packet[1+i*2].length_milliseconds = SYMBOL_LENGTH_MILLISECONDS*2;
        sounds_packet[1+i*2].number_of_frequencies = NUMBER_OF_CONCURRENT_CHANNELS;
        sounds_packet[1+i*2].frequencies[0] = CHANNEL_INDEX_TO_FREQUENCY(channels.a);
        sounds_packet[1+i*2].frequencies[1] = CHANNEL_INDEX_TO_FREQUENCY(channels.b);
        sounds_packet[1+i*2].frequencies[2] = CHANNEL_INDEX_TO_FREQUENCY(channels.c);

        channels = int_to_channels(SIGNAL_SEP+1);
        sounds_packet[2+i*2].length_milliseconds = SYMBOL_LENGTH_MILLISECONDS;
        sounds_packet[2+i*2].number_of_frequencies = NUMBER_OF_CONCURRENT_CHANNELS;
        sounds_packet[2+i*2].frequencies[0] = CHANNEL_INDEX_TO_FREQUENCY(channels.a);
        sounds_packet[2+i*2].frequencies[1] = CHANNEL_INDEX_TO_FREQUENCY(channels.b);
        sounds_packet[2+i*2].frequencies[2] = CHANNEL_INDEX_TO_FREQUENCY(channels.c);
    }

    channels = int_to_channels(SIGNAL_POST+2);
    sounds_packet[1 + 2 * size].length_milliseconds = SYMBOL_LENGTH_MILLISECONDS*2;
    sounds_packet[1 + 2 * size].number_of_frequencies = NUMBER_OF_CONCURRENT_CHANNELS;
    sounds_packet[1 + 2 * size].frequencies[0] = CHANNEL_INDEX_TO_FREQUENCY(channels.a);
    sounds_packet[1 + 2 * size].frequencies[1] = CHANNEL_INDEX_TO_FREQUENCY(channels.b);
    sounds_packet[1 + 2 * size].frequencies[2] = CHANNEL_INDEX_TO_FREQUENCY(channels.c);

    status = AUDIO__play_sounds(socket->audio, sounds_packet, 2+size*2);
    if (status != 0) {
        LOG_ERROR("Failed to play sounds");
        status = -1;
        goto l_cleanup;
    }


l_cleanup:
    if (sounds_packet != NULL) {
        free(sounds_packet);
        sounds_packet = NULL;
    }

    return status;
}


#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

ssize_t ASOCKET__recv(asocket_t* socket, void* data, size_t size) {
    getchar();
    memcpy(data, socket->buffer, min(size, socket->index + 1));
    return min(size, socket->index + 1);
}

