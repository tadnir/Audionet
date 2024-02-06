#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "physical_layer.h"

#include "audio/audio.h"
#include "logger.h"
#include "fft/fft.h"
#include "utils/minmax.h"
#include "audio_encoding.h"


#define SYMBOL_LENGTH_MILLISECONDS (150)


enum signals {

    /* 256 byte values 0-255 */

    SIGNAL_PREAMBLE = 270,
    SIGNAL_SEP = 275,
    SIGNAL_POST = 280,
    SIGNAL_MAX = 285 /* inclusive */
};

enum state_e {
    STATE_PREAMBLE,
    STATE_WORD,
    STATE_SEP,
};

struct audio_physical_layer_socket_s {
    audio_t* audio;
    fft_t* fft;
    enum state_e state;
    unsigned char votes[256];
    bool voted;
    char buffer[1024];
    int index;
};

static void listen_callback(audio_physical_layer_socket_t* socket, const float* recorded_frame, size_t size) {
    int ret;
    struct frequency_and_magnitude* frequencies = NULL;
    size_t count_frequencies;

    ret = FFT__calculate(socket->fft, recorded_frame, size, &frequencies, &count_frequencies);
    if (ret != 0) {
        LOG_ERROR("Failed to calculate fft on provided sound frame");
        return;
    }

    uint64_t value;
    ret = AUDIO_ENCODING__decode_frequencies(&value, count_frequencies, frequencies);
    if (ret == AUDIO_DECODE_RET_QUIET) {
        LOG_INFO("Quiet");
        return;
    } else if (ret != 0) {
        LOG_ERROR("Failed to decode frequencies");
        return;
    }

    switch (value) {
        case 0 ... 255:
            if (socket->state == STATE_WORD) {
                LOG_DEBUG("Data");
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
                LOG_DEBUG("Preamble");
                socket->index = 0;
                socket->state = STATE_WORD;
            }
            break;
        case SIGNAL_SEP ... SIGNAL_POST - 1:
            if (socket->state == STATE_WORD && socket->voted) {
                LOG_DEBUG("Sep");
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
        case SIGNAL_POST ... SIGNAL_MAX:
            if (socket->state != STATE_PREAMBLE) {
                LOG_DEBUG("Post");
                //TODO: output the accumulated buffer.
                socket->buffer[socket->index] = '\0';
                printf("buffer: <%s>", socket->buffer);
//                memset(socket->buffer, 0, 1024);
//                socket->index = 0;
                socket->state = STATE_PREAMBLE;
            }
            break;
        default:
            LOG_WARNING("Unknown signal %llu", value);
            break;
    }

l_cleanup:
    if (frequencies != NULL) {
        free((void*) frequencies);
        frequencies = NULL;
    }
}

audio_physical_layer_socket_t* PHYSICAL_LAYER__initialize() {
    audio_physical_layer_socket_t* socket = malloc(sizeof(audio_physical_layer_socket_t));
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
        PHYSICAL_LAYER__free(socket);
        return NULL;
    }

    return socket;
}

void PHYSICAL_LAYER__free(audio_physical_layer_socket_t* socket) {
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

int PHYSICAL_LAYER__listen(audio_physical_layer_socket_t* socket) {
    AUDIO__set_recording_callback(socket->audio, (recording_callback_t) listen_callback, socket);
    return 0;
}

int PHYSICAL_LAYER__send(audio_physical_layer_socket_t* socket, void* frame, size_t size) {
    int status = -1;

    struct sound* sounds_packet = malloc((2 + 2 * size) * sizeof(struct sound));
    if (sounds_packet == NULL) {
        LOG_ERROR("Failed to allocate sounds packet");
        return -1;
    }

    sounds_packet[0].length_milliseconds = SYMBOL_LENGTH_MILLISECONDS * 2;
    sounds_packet[0].number_of_frequencies = NUMBER_OF_CONCURRENT_CHANNELS;
    status = AUDIO_ENCODING__encode_frequencies(SIGNAL_PREAMBLE+1, NUMBER_OF_CONCURRENT_CHANNELS, sounds_packet[0].frequencies);
    if (status != 0) {
        LOG_ERROR("Failed to encode frequencies for value %d", SIGNAL_PREAMBLE);
        return status;
    }

    for (int frame_index = 0, packet_index = 1; frame_index < size; frame_index++, packet_index+=2) {
        sounds_packet[packet_index].length_milliseconds = SYMBOL_LENGTH_MILLISECONDS * 2;
        sounds_packet[packet_index].number_of_frequencies = NUMBER_OF_CONCURRENT_CHANNELS;
        status = AUDIO_ENCODING__encode_frequencies(((uint8_t*)frame)[frame_index], NUMBER_OF_CONCURRENT_CHANNELS, sounds_packet[packet_index].frequencies);
        if (status != 0) {
            LOG_ERROR("Failed to encode frequencies for value %d", SIGNAL_SEP);
            return status;
        }

        sounds_packet[packet_index + 1].length_milliseconds = SYMBOL_LENGTH_MILLISECONDS;
        sounds_packet[packet_index + 1].number_of_frequencies = NUMBER_OF_CONCURRENT_CHANNELS;
        status = AUDIO_ENCODING__encode_frequencies(SIGNAL_SEP+1, NUMBER_OF_CONCURRENT_CHANNELS, sounds_packet[packet_index+1].frequencies);
        if (status != 0) {
            LOG_ERROR("Failed to encode frequencies for value %d", SIGNAL_SEP);
            return status;
        }
    }

    sounds_packet[1 + 2 * size].length_milliseconds = SYMBOL_LENGTH_MILLISECONDS*2;
    sounds_packet[1 + 2 * size].number_of_frequencies = NUMBER_OF_CONCURRENT_CHANNELS;
    status = AUDIO_ENCODING__encode_frequencies(SIGNAL_POST+2, NUMBER_OF_CONCURRENT_CHANNELS, sounds_packet[1 + 2 * size].frequencies);
    if (status != 0) {
        LOG_ERROR("Failed to encode frequencies for value %d", SIGNAL_POST);
        return status;
    }

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

ssize_t PHYSICAL_LAYER__recv(audio_physical_layer_socket_t* socket, void* frame, size_t size) {
    getchar();
    memcpy(frame, socket->buffer, min(size, socket->index + 1));
    return min(size, socket->index + 1);
}

