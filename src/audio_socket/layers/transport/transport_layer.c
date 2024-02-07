#include <stdint.h>
#include <malloc.h>
#include <string.h>

#include "transport_layer.h"
#include "audio_socket/layers/link/link_layer.h"
#include "logger.h"
#include "utils/minmax.h"

#define TRANSPORT_PACKET_DATA_LENGTH_LIMIT (1024)

struct audio_transport_layer_socket_s {
    audio_link_layer_socket_t* link_layer;
    uint8_t seq;
};

struct transport_packet_header_s {
    uint8_t seq;
};

struct transport_packet_s {
    struct transport_packet_header_s header;
    uint8_t data[TRANSPORT_PACKET_DATA_LENGTH_LIMIT];
} __attribute__((packed));

audio_transport_layer_socket_t *TRANSPORT_LAYER__initialize() {
    audio_transport_layer_socket_t* socket = malloc(sizeof(audio_transport_layer_socket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to allocate audio transport socket");
        return NULL;
    }

    socket->link_layer = LINK_LAYER__initialize();
    if (socket->link_layer == NULL) {
        LOG_ERROR("Failed to initialize audio link layer");
        free(socket);
        return NULL;
    }

    socket->seq = 0;
    return socket;
}

void TRANSPORT_LAYER__free(audio_transport_layer_socket_t *socket) {
    LINK_LAYER__free(socket->link_layer);
    free(socket);
}

int TRANSPORT_LAYER__send(audio_transport_layer_socket_t *socket, void *data, size_t size) {
    int ret = -1;
    ssize_t recv_ret = -1;
    size_t remaining = size;
    uint8_t* current_data = data;
    struct transport_packet_s packet_out;
    struct transport_packet_s packet_in;

    packet_out.header.seq = socket->seq;
    memcpy(packet_out.data, current_data, min(remaining, TRANSPORT_PACKET_DATA_LENGTH_LIMIT));
    while (remaining > 0) {
        ret = LINK_LAYER__send(socket->link_layer, &packet_out, min(remaining + sizeof(struct transport_packet_header_s), sizeof(packet_out)));
        if (ret != 0) {
            LOG_ERROR("Failed to send on link layer");
            return -1;
        }

        recv_ret = LINK_LAYER__recv(socket->link_layer, &packet_in, sizeof(packet_in));
        if (recv_ret == RECV_TIMEOUT_RET_CODE) {
            LOG_INFO("Timed out, retrying send");
            continue;
        } else if (recv_ret < 0) {
            LOG_ERROR("Failed to recv ack on transport layer");
            return -1;
        }

        if (packet_in.header.seq == packet_out.header.seq) {
            socket->seq++;
            packet_out.header.seq = socket->seq;
            remaining -= recv_ret;
            current_data += recv_ret;
            memcpy(packet_out.data, current_data, min(remaining, TRANSPORT_PACKET_DATA_LENGTH_LIMIT));
        }
    }

    return 0;
}

ssize_t TRANSPORT_LAYER__recv(audio_transport_layer_socket_t *socket, void *data, size_t size) {
    int ret = -1;
    ssize_t recv_ret = -1;
    struct transport_packet_header_s packet_out;
    struct transport_packet_s packet_in;

    while (true) {
        recv_ret = LINK_LAYER__recv(socket->link_layer, &packet_in, sizeof(packet_in));
        if (recv_ret == RECV_TIMEOUT_RET_CODE) {
            LOG_WARNING("Timed out on recv");
            continue;
        } else if (recv_ret < 0) {
            LOG_ERROR("Failed to recv on transport layer: %zd", recv_ret);
            return recv_ret;
        }

        if (packet_in.header.seq != socket->seq) {
            LOG_WARNING("Bad seq %d", packet_in.header.seq);
            continue;
        }

        packet_out.seq = packet_in.header.seq;
        socket->seq++;
        ret = LINK_LAYER__send(socket->link_layer, &packet_out, sizeof(packet_out));
        if (ret != 0) {
            LOG_ERROR("Failed to send transport layer ack");
            return -1;
        }
    }
}
