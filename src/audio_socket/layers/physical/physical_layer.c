#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "physical_layer.h"

#include "audio/audio.h"
#include "utils/logger.h"
#include "fft/fft.h"
#include "utils/utils.h"
#include "audio_encoding.h"

/** The length of time each value symbol will sound. */
#define SYMBOL_LENGTH_MILLISECONDS (150)

/** The length of time each preamble symbol will sound. */
#define PREAMBLE_SYMBOL_LENGTH_MILLISECONDS (SYMBOL_LENGTH_MILLISECONDS * 2)

/** The length of time each post symbol will sound. */
#define POST_SYMBOL_LENGTH_MILLISECONDS (SYMBOL_LENGTH_MILLISECONDS * 2)

/** The length of time each seperator symbol will sound. */
#define SEP_SYMBOL_LENGTH_MILLISECONDS (SYMBOL_LENGTH_MILLISECONDS)

/** The maximum amount of frames to be cached */
#define MAX_FRAMES_COUNT (50)

/**
 * Defines the numerical value of each symbol, while data symbols are their own value.
 */
enum signals {

    /* 256 byte values 0-255 */

    /** Preamble symbol code */
    SIGNAL_PREAMBLE = 270,

    /** Seperator symbol code */
    SIGNAL_SEP = 275,

    /** Post symbol code */
    SIGNAL_POST = 280,

    /** The maximal symbol value (inclusive) */
    SIGNAL_MAX = 285
};

/**
 * The current state in the receive state machine.
 */
enum state_e {
    /** Waiting for preamble */
    STATE_PREAMBLE,

    /** Collecting data */
    STATE_WORD,

    /** Discarding incoming symbols */
    STATE_DISCARDING,
};


/**
 * Contains the data that has been (or currently is) received for a single packet.
 */
struct packet_buffer {
    /** The current amount of bytes filled into `buffer`. */
    uint32_t packet_size;

    /** Whether the packet has been full received and is considered ready. */
    bool is_ready;

    /** Buffer containing the packet received. */
    uint8_t buffer[PHYSICAL_LAYER_MTU];
};

struct audio_physical_layer_socket_s {
    /** The audio module for recording/playback. */
    audio_t* audio;

    /** The FFT module for recorded data decoding. */
    fft_t* fft;

    /** The current state in the state machine. */
    enum state_e state;

    /** The current byte's votes. */
    int byte_votes[256];

    /** Whether there's been a voting for the current byte. */
    bool is_byte_voted;

    /** Array of buffer structs, each one contains a packet and it's state. */
    struct packet_buffer packet_buffers[MAX_FRAMES_COUNT];

    /** The index of the packet buffer that's currently being written. */
    int packet_write_index;

    /** The index of the packet buffer that's to be read when ready. */
    int packet_read_index;

    /** The configured timeout for recv operation. */
    int recv_timeout_seconds;
};

/**
 * Takes a recording and tries to decode it's frequencies into an integer value.
 *
 * @param fft The FFT engine for getting frequencies from the recording.
 * @param recorded_frame The recorded sound data.
 * @param size The length of the recorded data buffer.
 * @param value_out Returns the decoded value from the recoded data.
 * @return 0 on Success, -1 on Failure.
 */
static int decode_recording(fft_t* fft, const float* recorded_frame, size_t size, uint64_t* value_out) {
    int ret;
    size_t count_frequencies;
    struct frequency_and_magnitude* frequencies = NULL;

    /* Get the frequencies in from the recording. */
    ret = FFT__calculate(fft, recorded_frame, size, &frequencies, &count_frequencies);
    if (ret != 0) {
        LOG_ERROR("Failed to calculate fft on provided sound frame");
        return -1;
    }

    /* Decode the recording. */
    ret = AUDIO_ENCODING__decode_frequencies(value_out, count_frequencies, frequencies);
    free(frequencies);
    if (ret == AUDIO_DECODE_RET_QUIET) {
        LOG_VERBOSE("Quiet");
    } else if (ret != 0) {
        LOG_ERROR("Failed to decode frequencies");
    }

    return ret;
}


/**
 * This function will be registered as an audio listener for the audio module.
 * For each recording given, will decoded the audio frame and execute the state machine step,
 * updating relevant packet buffers.
 *
 * @param socket The socket context for the callback.
 * @param recorded_frame The audio frame recorded.
 * @param size The size of the recorded frame.
 */
static void listen_callback(audio_physical_layer_socket_t* socket, const float* recorded_frame, size_t size) {
    /* Try to decoded the audio frame. */
    uint64_t value;
    int ret = decode_recording(socket->fft, recorded_frame, size, &value);
    if (ret != 0) {
        return;
    }

    /* Depending on the value decoded and the current state machine status, make a step and updates. */
    switch (value) {
        /* These values signify data, updates the votes (only in WORD state). */
        case 0 ... 255:
            if (socket->state == STATE_WORD) {
                /* Update the votes, and flag that the current byte has been voted at least once. */
                socket->is_byte_voted = true;
                socket->byte_votes[value]++;
            }
            break;

        /* Handle a preamble signal depending on the current state. */
        case SIGNAL_PREAMBLE ... SIGNAL_SEP-1:
            if (socket->state == STATE_PREAMBLE) {
                LOG_DEBUG("Preamble");
                if (socket->packet_buffers[socket->packet_write_index].is_ready) {
                    /* The current buffer is ready and wasn't finished properly, start discarding. */
                    LOG_DEBUG("Preamble with full buffer -> discarding");
                    socket->state = STATE_DISCARDING;
                } else {
                    /* Starting new buffer, expect data. */
                    socket->state = STATE_WORD;
                    socket->packet_buffers[socket->packet_write_index].packet_size = 0;
                }
            }
            break;

        /* Handle a seperator signal depending on the current state. */
        case SIGNAL_SEP ... SIGNAL_POST - 1:
            if (socket->state == STATE_WORD && socket->is_byte_voted) {
                /* We finished a byte vote. */
                socket->is_byte_voted = false;
                struct packet_buffer* buffer = &socket->packet_buffers[socket->packet_write_index];

                /* Validate the packet size. */
                if (buffer->packet_size >= PHYSICAL_LAYER_MTU) {
                    socket->state = STATE_DISCARDING;
                } else {
                    /* Register the vote winner, and advance the buffer size index. */
                    buffer->buffer[buffer->packet_size] = find_max_index(256, socket->byte_votes);
                    LOG_DEBUG("data: %hhu (%c)", buffer->buffer[buffer->packet_size], buffer->buffer[buffer->packet_size]);
                    buffer->packet_size++;
                }

                /* Clear the votes. */
                memset(socket->byte_votes, 0, sizeof(socket->byte_votes));
                LOG_DEBUG("Sep");
            }
            break;

        /* Handle a post signal depending on the current state. */
        case SIGNAL_POST ... SIGNAL_MAX:
            if (socket->state == STATE_DISCARDING || socket->state == STATE_PREAMBLE) {
                /* Restart packet */
                if (!socket->packet_buffers[socket->packet_write_index].is_ready) {
                    socket->packet_buffers[socket->packet_write_index].packet_size = 0;
                }
                socket->state = STATE_PREAMBLE;
            } else {
                LOG_DEBUG("Post");

                /* Finalize the packet buffer and advance the write index. */
                struct packet_buffer* buffer = &socket->packet_buffers[socket->packet_write_index];
                if (buffer->packet_size > 0) {
                    socket->packet_write_index = (socket->packet_write_index + 1) % MAX_FRAMES_COUNT;
                    buffer->is_ready = true;
                }

                /* Clear the votes. */
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
    /* Allocate a socket struct. */
    audio_physical_layer_socket_t* socket = malloc(sizeof(audio_physical_layer_socket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to initialize Audio Socket");
        return NULL;
    }

    /* Initialize socket fields. */
    socket->state = STATE_PREAMBLE;
    memset(socket->byte_votes, 0, sizeof(socket->byte_votes));
    socket->is_byte_voted = false;
    memset(socket->packet_buffers, 0, sizeof(socket->packet_buffers));
    socket->packet_write_index = 0;
    socket->packet_read_index = 0;
    socket->recv_timeout_seconds = RECV_TIMEOUT_SECONDS;

    /* Initialize the FFT module. */
    //TODO: Set this value meaningfully.
    socket->fft = FFT__initialize(3600, SAMPLE_RATE_48000);
    if (socket->fft == NULL) {
        LOG_ERROR("Failed to initialize fft");
        free(socket);
        return NULL;
    }

    /* Initialize Audio module. */
    socket->audio = AUDIO__initialize(SAMPLE_RATE_48000, false);
    if (socket->audio == NULL) {
        LOG_ERROR("Failed to initialize audio");
        FFT__free(socket->fft);
        free(socket);
        return NULL;
    }

    /* Set the listening callback and start listening. */
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
    /* Stop and free the audio module. */
    if (socket->audio != NULL) {
        (void)AUDIO__stop(socket->audio);
        AUDIO__free(socket->audio);
        socket->audio = NULL;
    }

    /* Free the FFT module. */
    if (socket->fft != NULL) {
        FFT__free(socket->fft);
        socket->fft = NULL;
    }

    /* Free the socket struct. */
    free(socket);
}

/**
 * Sets a sound to the encoded frequencies of a given integer value.
 *
 * @param sound The sound to set.
 * @param length_milliseconds The sound length to set.
 * @param number_of_frequencies The number of frequencies in the sound.
 * @param value The integer value to set.
 * @return 0 On Success, -1 On Failure.
 */
static int set_sound_by_value(struct sound_s* sound, uint32_t length_milliseconds, uint32_t number_of_frequencies, int64_t value) {
    /* Set fields. */
    sound->length_milliseconds = length_milliseconds;
    sound->number_of_frequencies = number_of_frequencies;

    /* Encode the integer value to sound frequencies. */
    int status = AUDIO_ENCODING__encode_frequencies(value, number_of_frequencies, sound->frequencies);
    if (status != 0) {
        LOG_ERROR("Failed to encode frequencies for value %lld", value);
    }

    return status;
}

int PHYSICAL_LAYER__send(audio_physical_layer_socket_t* socket, void* frame, size_t size) {
    int status = -1;

    /* Validate parameters. */
    if (size == 0 || frame == NULL || size > PHYSICAL_LAYER_MTU) {
        LOG_ERROR("Bad Parameters");
        return -1;
    }

    /* The maximum amount of sounds we need to sound in order to send a frame,
     * is double the MTU (1 sound for data, 1 sound for sep, for each byte) plus 2 (PRE + POST).  */
    struct sound_s sounds_packet[2 + 2 * PHYSICAL_LAYER_MTU];

    /* Set the PREAMBLE sound.
     * We defined some tolerances for the signaling sounds, a +1 will give better results */
    status = set_sound_by_value(&sounds_packet[0], PREAMBLE_SYMBOL_LENGTH_MILLISECONDS,
                                NUMBER_OF_CONCURRENT_CHANNELS,SIGNAL_PREAMBLE+1);
    if (status != 0) {
        return status;
    }

    /* For each byte, set the data sound and the SEP sound.
     * +1 for the tolerance enhancement as before. */
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

    /* Set the POST sound (+1 as before). */
    status = set_sound_by_value(&sounds_packet[1 + 2 * size], POST_SYMBOL_LENGTH_MILLISECONDS,
                                NUMBER_OF_CONCURRENT_CHANNELS, SIGNAL_POST+1);
    if (status != 0) {
        return status;
    }

    /* Play the sounds, effectively sending the frame. */
    status = AUDIO__play_sounds(socket->audio, sounds_packet, 2+size*2);
    if (status != 0) {
        LOG_ERROR("Failed to play sounds");
        return status;
    }

    return 0;
}

ssize_t PHYSICAL_LAYER__peek(audio_physical_layer_socket_t* socket, void* frame, size_t size, bool blocking) {
    /* Validate parameters. */
    if (size < PHYSICAL_LAYER_MTU || frame == NULL) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }

    /* Get the current read packet buffer, and check repeatedly (up to timeout) whether it's ready. */
    struct packet_buffer *packet = &socket->packet_buffers[socket->packet_read_index];
    for (int i = 0; i <= socket->recv_timeout_seconds; ++i) {
        if (i > 0) {
            /* Sleep a single second. */
            sleep(1);
        }

        /* If the buffer is ready we can return it. */
        if (packet->is_ready) {
            uint32_t packet_size = min(packet->packet_size, PHYSICAL_LAYER_MTU);
            memcpy(frame, packet->buffer, packet_size);
            return packet_size;
        }

        if (!blocking) {
            /* There's no buffer and timeout is irrelevant,
             * this isn't an error state so we return 0 just to signify there's no ready buffers.
             * If there were, they'd have a positive size. */
            return 0;
        }
    }

    /* Timeout reached. */
    LOG_ERROR("Timed out on physical layer peek");
    return RECV_TIMEOUT_RET_CODE;
}

int PHYSICAL_LAYER__pop(audio_physical_layer_socket_t* socket) {
    /* If there's a ready read packet, set it as not ready and advance the index. */
    struct packet_buffer *packet = &socket->packet_buffers[socket->packet_read_index];
    if (packet->is_ready) {
        packet->packet_size = 0;
        packet->is_ready = false;
        socket->packet_read_index = (socket->packet_read_index + 1) % MAX_FRAMES_COUNT;
        return 0;
    }

    /* There's no ready packet so we failed popping. */
    return -1;
}

ssize_t PHYSICAL_LAYER__recv(audio_physical_layer_socket_t* socket, void* frame, size_t size) {
    /* Validate parameters. */
    if (size < PHYSICAL_LAYER_MTU || frame == NULL) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }

    /* Peek-wait for a packet, effectively receiving it. */
    ssize_t ret = PHYSICAL_LAYER__peek(socket, frame, size, true);
    if (ret < 0) {
        return ret;
    } else if (ret == 0) {
        return RECV_TIMEOUT_RET_CODE;
    }

    /* Since we successfully peeked, there's definitely a packet to pop. */
    PHYSICAL_LAYER__pop(socket);
    return ret;
}

