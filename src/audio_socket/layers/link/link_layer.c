#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "link_layer.h"
#include "audio_socket/layers/physical/physical_layer.h"
#include "utils/logger.h"
#include "utils/utils.h"

/** The maximum amount of frames the link packet can be split into. */
#define MAX_LINK_FRAMES (UCHAR_MAX + 1)

/** The maximum size of a single link packet (over multiple physical frames). */
#define MAX_LINK_PACKET_SIZE (MAX_LINK_FRAMES * (PHYSICAL_LAYER_MTU - 1))

struct audio_link_layer_socket_s {
    /** The link layer uses the physical layer to send frames. */
    audio_physical_layer_socket_t* physical_layer;
};

/**
 * The structure of a single physical frame sent/received as part of the link packet.
 */
struct link_frame_s {
    /** The sequence number of the frame. */
    uint8_t seq;

    /** The data carried by the frame. */
    uint8_t data[PHYSICAL_LAYER_MTU - 1];
} __attribute__((packed));

/**
 * The structure of a link packet header, Will be set before the other frames.
 */
struct link_packet_header_s {
    /** The header describes the total length of data that is carried over multiple frames. */
    uint32_t data_length;
} __attribute__((packed));

audio_link_layer_socket_t *LINK_LAYER__initialize() {
    /* Allocate the link layer socket struct. */
    audio_link_layer_socket_t* socket = malloc(sizeof(audio_link_layer_socket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to allocate link layer audio socket");
        return NULL;
    }

    /* Initialize the physical layer */
    socket->physical_layer = PHYSICAL_LAYER__initialize();
    if (socket->physical_layer == NULL) {
        LOG_ERROR("Failed to initialize audio physical layer");
        free(socket);
        return NULL;
    }

    return socket;
}

void LINK_LAYER__free(audio_link_layer_socket_t *socket) {
    /* Free the physical layer */
    PHYSICAL_LAYER__free(socket->physical_layer);

    /* Free the link layer */
    free(socket);
}

int LINK_LAYER__send(audio_link_layer_socket_t *socket, void *data, size_t size) {
    /* Validate parameters. */
    if (size > MAX_LINK_PACKET_SIZE - sizeof(struct link_packet_header_s)) {
        LOG_ERROR("link packet exceeds maximum size");
        return -1;
    }

    // Note: Here we could add CRC calculation and add to the packet header.

    /* Create the packet header and the first frame. */
    int ret;
    struct link_frame_s frame = { .seq = 0 };
    struct link_packet_header_s header;
    header.data_length = size;

    /* Send the data frame by frame until finished. */
    size_t header_sent = 0;
    size_t data_sent = 0;
    while (data_sent < size) {
        size_t frame_data_length = 0;
        /* If the header hasn't been sent yet, copy to the frame. */
        if (header_sent < sizeof(header)) {
            size_t header_length_to_be_sent = min(PHYSICAL_LAYER_MTU - 1, sizeof(header) - header_sent);
            memcpy(frame.data, ((uint8_t*)&header) + header_sent, header_length_to_be_sent);
            frame_data_length += header_length_to_be_sent;
            header_sent += header_length_to_be_sent;
        }

        /* Add as much data as possible to the frame. */
        if (frame_data_length < PHYSICAL_LAYER_MTU - 1) {
            size_t data_length_to_be_sent = min(PHYSICAL_LAYER_MTU - 1 - frame_data_length, size - data_sent);
            memcpy(((uint8_t*)frame.data) + frame_data_length, ((uint8_t*)data) + data_sent, data_length_to_be_sent);
            frame_data_length += data_length_to_be_sent;
            data_sent += data_length_to_be_sent;
        }

        /* Send the frame */
        ret = PHYSICAL_LAYER__send(socket->physical_layer, &frame, frame_data_length + 1);
        if (ret != 0) {
            LOG_ERROR("Failed to send data on physical layer");
            return ret;
        }

        frame.seq++;
    }

    return 0;
}

ssize_t LINK_LAYER__recv(audio_link_layer_socket_t *socket, void *data, size_t size) {
    struct link_frame_s frame;
    struct link_packet_header_s header;
    size_t header_written = 0;
    size_t data_written = 0;
    size_t current_new_data_count = 0;
    uint8_t seq = 0;

    while (true) {
        /* Get the next frame. */
        ssize_t recv_ret = PHYSICAL_LAYER__recv(socket->physical_layer, &frame, PHYSICAL_LAYER_MTU);
        if (recv_ret < 0) {
            LOG_ERROR("Failed to recv link layer header: %zd", recv_ret);
            return recv_ret;
        }

        /* Check the sequence of the received frame. */
        if (seq != frame.seq) {
            LOG_ERROR("link layer received bad seq %d, expected %d, cleaning physical layer", frame.seq, seq);
            /* We got a bad sequence number, so we pop all the next frames until we find a `0` sequence frame */
            while (true) {
                recv_ret = PHYSICAL_LAYER__peek(socket->physical_layer, &frame, PHYSICAL_LAYER_MTU, false);
                if (recv_ret < 0 ) {
                    return recv_ret;
                } else if (recv_ret == 0 || frame.seq == 0) {
                    /* We cleaned all the frames until a `0` frame, return out-of-sync */
                    return RECV_OUT_OF_SYNC_RET_CODE;
                }

                PHYSICAL_LAYER__pop(socket->physical_layer);
            }
        }

        seq++;
        current_new_data_count = recv_ret - 1;

        /* Write the data to the header first, if not filled */
        size_t amount_to_write_to_header = 0;
        if (header_written < sizeof(header)) {
            amount_to_write_to_header = min(current_new_data_count, sizeof(header) - header_written);
            memcpy(((uint8_t*)&header) + header_written, frame.data, amount_to_write_to_header);
            header_written += amount_to_write_to_header;
            current_new_data_count -= amount_to_write_to_header;
        }

        /* Write the rest of the data to the output buffer */
        if (current_new_data_count > 0) {
            size_t amount_to_write_to_data = min(current_new_data_count, size - data_written);
            memcpy(((uint8_t*)data) + data_written, frame.data + amount_to_write_to_header, amount_to_write_to_data);
            data_written += amount_to_write_to_data;
        }

        /* We finish when we filled the header and received `data_length` bytes */
        if (header_written >= sizeof(header) && header.data_length <= data_written) {
            break;
        }
    }

    return (ssize_t) data_written;
}
