#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "physical_layer.h"

#include "audio/audio.h"
#include "logger.h"
#include "fft/fft.h"
#include "utils/minmax.h"
#include "audio_encoding.h"
#include "utils/utils.h"


#define SYMBOL_LENGTH_MILLISECONDS (150)
#define PREAMBLE_SYMBOL_LENGTH_MILLISECONDS (SYMBOL_LENGTH_MILLISECONDS * 2)
#define POST_SYMBOL_LENGTH_MILLISECONDS (SYMBOL_LENGTH_MILLISECONDS * 2)
#define SEP_SYMBOL_LENGTH_MILLISECONDS (SYMBOL_LENGTH_MILLISECONDS)
#define BUFFER_COUNT (50)

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
    STATE_DISCARDING,
};

struct packet_buffer {
    uint32_t packet_size;
    bool is_full;
    uint8_t buffer[PHYSICAL_LAYER_MTU];
};

struct audio_physical_layer_socket_s {
    audio_t* audio;
    fft_t* fft;
    enum state_e state;
    int byte_votes[256];
    bool is_byte_voted;
    struct packet_buffer packet_buffers[BUFFER_COUNT];
    int packet_write_index;
    int packet_read_index;
    int recv_timeout_seconds;
};

static int decode_recording(fft_t* fft, const float* recorded_frame, size_t size, uint64_t* value_out) {
    int ret;
    size_t count_frequencies;
    struct frequency_and_magnitude* frequencies = NULL;

    ret = FFT__calculate(fft, recorded_frame, size, &frequencies, &count_frequencies);
    if (ret != 0) {
        LOG_ERROR("Failed to calculate fft on provided sound frame");
        return -1;
    }

    ret = AUDIO_ENCODING__decode_frequencies(value_out, count_frequencies, frequencies);
    free(frequencies);
    if (ret == AUDIO_DECODE_RET_QUIET) {
        LOG_VERBOSE("Quiet");
    } else if (ret != 0) {
        LOG_ERROR("Failed to decode frequencies");
    }

    return ret;
}

static void listen_callback(audio_physical_layer_socket_t* socket, const float* recorded_frame, size_t size) {
    uint64_t value;
    int ret = decode_recording(socket->fft, recorded_frame, size, &value);
    if (ret != 0) {
        return;
    }

    switch (value) {
        case 0 ... 255:
            if (socket->state == STATE_WORD) {
                socket->is_byte_voted = true;
                socket->byte_votes[value]++;
#ifdef DEBUG
                char temp[2048];
                int j = 0;
                for (int i = 0; i < 256; ++i) {
                    if (socket->byte_votes[i] != 0) {
                        snprintf(temp + j * 5, 2048 - j * 5, "%03d, ", i);
                        j++;
                    }
                }
                temp[j*5+1] = '\0';
                LOG_VERBOSE("%s;", temp);
                j=0;
                for (int i = 0; i < 256; ++i) {
                    if (socket->byte_votes[i] != 0) {
                        snprintf(temp + j * 5, 2048 - j * 5, "%03d, ", socket->byte_votes[i]);
                        j++;
                    }
                }
                temp[j*5+1] = '\0';
                LOG_VERBOSE("%s;", temp);
#endif
            }
            break;
        case SIGNAL_PREAMBLE ... SIGNAL_SEP-1:
            switch (socket->state) {
                case STATE_PREAMBLE:
                    LOG_DEBUG("Preamble");
                    if (socket->packet_buffers[socket->packet_write_index].is_full) {
                        LOG_DEBUG("Preamble with full buffer -> discarding");
                        socket->state = STATE_DISCARDING;
                    } else {
                        socket->state = STATE_WORD;
                        socket->packet_buffers[socket->packet_write_index].packet_size = 0;
                    }
            }
            break;
        case SIGNAL_SEP ... SIGNAL_POST - 1:
            if (socket->state == STATE_WORD && socket->is_byte_voted) {
                socket->is_byte_voted = false;
                struct packet_buffer* buffer = &socket->packet_buffers[socket->packet_write_index];
                if (buffer->packet_size >= PHYSICAL_LAYER_MTU) {
                    socket->state = STATE_DISCARDING;
                } else {
                    buffer->buffer[buffer->packet_size] = find_max_index(256, socket->byte_votes);
                    LOG_DEBUG("data: %hhu (%c)", buffer->buffer[buffer->packet_size], buffer->buffer[buffer->packet_size]);
                    buffer->packet_size++;
                }
                memset(socket->byte_votes, 0, sizeof(socket->byte_votes));
                LOG_DEBUG("Sep");
            }
            break;
        case SIGNAL_POST ... SIGNAL_MAX:
            if (socket->state == STATE_DISCARDING || socket->state == STATE_PREAMBLE) {
                if (!socket->packet_buffers[socket->packet_write_index].is_full) {
                    socket->packet_buffers[socket->packet_write_index].packet_size = 0;
                }
                socket->state = STATE_PREAMBLE;
            } else {
                LOG_DEBUG("Post");
                struct packet_buffer* buffer = &socket->packet_buffers[socket->packet_write_index];
                if (buffer->packet_size > 0) {
                    socket->packet_write_index = (socket->packet_write_index + 1) % BUFFER_COUNT;
                    buffer->is_full = true;
                }
                memset(socket->byte_votes, 0, sizeof(socket->byte_votes));
                socket->is_byte_voted = false;
                socket->state = STATE_PREAMBLE;
            }
            break;
        default:
            LOG_WARNING("Unknown signal %llu", value);
            break;
    }
}

audio_physical_layer_socket_t* PHYSICAL_LAYER__initialize() {
    audio_physical_layer_socket_t* socket = malloc(sizeof(audio_physical_layer_socket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to initialize Audio Socket");
        return NULL;
    }

    socket->state = STATE_PREAMBLE;
    memset(socket->byte_votes, 0, sizeof(socket->byte_votes));
    socket->is_byte_voted = false;
    memset(socket->packet_buffers, 0, sizeof(socket->packet_buffers));
    socket->packet_write_index = 0;
    socket->packet_read_index = 0;
    socket->recv_timeout_seconds = RECV_TIMEOUT_SECONDS;

    LOG_DEBUG("Initializing FFT");
    socket->fft = FFT__initialize(3600, SAMPLE_RATE_48000);
    if (socket->fft == NULL) {
        LOG_ERROR("Failed to initialize fft");
        free(socket);
        return NULL;
    }

    LOG_DEBUG("Initializing Audio");
    socket->audio = AUDIO__initialize(SAMPLE_RATE_48000, false);
    if (socket->audio == NULL) {
        LOG_ERROR("Failed to initialize audio");
        FFT__free(socket->fft);
        free(socket);
        return NULL;
    }

    LOG_DEBUG("Starting Audio");
    AUDIO__set_recording_callback(socket->audio, (recording_callback_t) listen_callback, socket);
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

static int set_sound_by_value(struct sound_s* sound, uint32_t length_milliseconds, uint32_t number_of_frequencies, int64_t value) {
    sound->length_milliseconds = length_milliseconds;
    sound->number_of_frequencies = number_of_frequencies;
    int status = AUDIO_ENCODING__encode_frequencies(value, number_of_frequencies, sound->frequencies);
    if (status != 0) {
        LOG_ERROR("Failed to encode frequencies for value %lld", value);
    }
    return status;
}

int PHYSICAL_LAYER__send(audio_physical_layer_socket_t* socket, void* frame, size_t size) {
    int status = -1;
    if (size == 0 || frame == NULL || size > PHYSICAL_LAYER_MTU) {
        LOG_ERROR("Bad Parameters");
        return -1;
    }

    struct sound_s sounds_packet[2 + 2 * PHYSICAL_LAYER_MTU];
    status = set_sound_by_value(&sounds_packet[0], PREAMBLE_SYMBOL_LENGTH_MILLISECONDS,
                                NUMBER_OF_CONCURRENT_CHANNELS, SIGNAL_PREAMBLE+1);
    if (status != 0) {
        return status;
    }

    for (int frame_index = 0, packet_index = 1; frame_index < size; frame_index++, packet_index+=2) {
        status = set_sound_by_value(&sounds_packet[packet_index], SYMBOL_LENGTH_MILLISECONDS,
                                    NUMBER_OF_CONCURRENT_CHANNELS, ((uint8_t*)frame)[frame_index]);
        if (status != 0) {
            return status;
        }

        status = set_sound_by_value(&sounds_packet[packet_index + 1], SEP_SYMBOL_LENGTH_MILLISECONDS,
                                    NUMBER_OF_CONCURRENT_CHANNELS, SIGNAL_SEP+1);
        if (status != 0) {
            return status;
        }
    }

    status = set_sound_by_value(&sounds_packet[1 + 2 * size], POST_SYMBOL_LENGTH_MILLISECONDS,
                                NUMBER_OF_CONCURRENT_CHANNELS, SIGNAL_POST+2);
    if (status != 0) {
        return status;
    }

    status = AUDIO__play_sounds(socket->audio, sounds_packet, 2+size*2);
    if (status != 0) {
        LOG_ERROR("Failed to play sounds");
        return status;
    }

    return 0;
}

ssize_t PHYSICAL_LAYER__peek(audio_physical_layer_socket_t* socket, void* frame, size_t size, bool blocking) {
    if (size < PHYSICAL_LAYER_MTU || frame == NULL) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }

    struct packet_buffer *packet = &socket->packet_buffers[socket->packet_read_index];
    for (int i = 0; i <= socket->recv_timeout_seconds; ++i) {
        if (i > 0) {
            sleep(1);
        }

        if (packet->is_full) {
            uint32_t packet_size = min(packet->packet_size, PHYSICAL_LAYER_MTU);
            memcpy(frame, packet->buffer, packet_size);
            return packet_size;
        }

        if (!blocking) {
            return 0;
        }
    }

    LOG_ERROR("Timed out on physical layer peek");
    return RECV_TIMEOUT_RET_CODE;
}

void PHYSICAL_LAYER__pop(audio_physical_layer_socket_t* socket) {
    struct packet_buffer *packet = &socket->packet_buffers[socket->packet_read_index];
    if (packet->is_full) {
        packet->packet_size = 0;
        packet->is_full = false;
        socket->packet_read_index = (socket->packet_read_index + 1) % BUFFER_COUNT;
    }
}

ssize_t PHYSICAL_LAYER__recv(audio_physical_layer_socket_t* socket, void* frame, size_t size) {
    if (size < PHYSICAL_LAYER_MTU || frame == NULL) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }

    ssize_t ret = PHYSICAL_LAYER__peek(socket, frame, size, true);
    if (ret < 0) {
        return ret;
    } else if (ret == 0) {
        return RECV_TIMEOUT_RET_CODE;
    }

    PHYSICAL_LAYER__pop(socket);
    return ret;
}

