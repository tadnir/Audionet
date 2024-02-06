#include <malloc.h>
#include <stdint.h>
#include <string.h>

#include "link_layer.h"
#include "audio_socket/layers/physical/physical_layer.h"
#include "logger.h"
#include "utils/minmax.h"

struct audio_link_layer_socket_s {
    audio_physical_layer_socket_t* physical_layer;
};

struct link_packet_header_s {
    uint64_t data_length;

    uint8_t data[];
} __attribute__((packed));

audio_link_layer_socket_t *LINK_LAYER__initialize() {
    audio_link_layer_socket_t* socket = malloc(sizeof(audio_link_layer_socket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to allocate link layer audio socket");
        return NULL;
    }

    socket->physical_layer = PHYSICAL_LAYER__initialize();
    if (socket->physical_layer == NULL) {
        free(socket);
        return NULL;
    }

    return socket;
}

void LINK_LAYER__free(audio_link_layer_socket_t *socket) {
    PHYSICAL_LAYER__free(socket->physical_layer);
    free(socket);
}

int LINK_LAYER__listen(audio_link_layer_socket_t *socket) {
    return PHYSICAL_LAYER__listen(socket->physical_layer);
}

int LINK_LAYER__send(audio_link_layer_socket_t *socket, void *data, size_t size) {
    size_t packet_length = sizeof(struct link_packet_header_s) + size;
    struct link_packet_header_s* packet = malloc(packet_length);
    if (packet == NULL) {
        LOG_ERROR("Failed to allocate link send packet");
        return -1;
    }

    memcpy(packet->data, data, size);
    packet->data_length = size;

    int ret = 0;
    for (size_t i = 0; i < packet_length; i+=MTU) {
        size_t size_to_send = min(MTU, packet_length - i);
        ret = PHYSICAL_LAYER__send(socket->physical_layer, packet + i, size_to_send);
        if (ret != 0) {
            LOG_ERROR("Failed to send data on physical layer, managed to send %zu", i * MTU);
            free(packet);
            return ret;
        }
    }

    free(packet);
    return 0;
}

#include <unistd.h>
ssize_t LINK_LAYER__recv(audio_link_layer_socket_t *socket, void *data, size_t size) {
    uint8_t packet_buffer[MTU * 2];
    size_t total_recv = 0;
    while (total_recv < sizeof(struct link_packet_header_s)) {
        ssize_t recv_ret = PHYSICAL_LAYER__recv(socket->physical_layer, packet_buffer + total_recv,MTU);
        if (recv_ret < 0) {
            LOG_ERROR("Failed to recv link layer header");
            return -1;
        }

        total_recv += recv_ret;
    }

    struct link_packet_header_s header = *(struct link_packet_header_s *) packet_buffer;
    LOG_INFO("HERE %llu %llu", sizeof(struct link_packet_header_s), header.data_length);
    LOG_INFO("RECV %zu", total_recv);
    total_recv -= sizeof(struct link_packet_header_s);
    LOG_INFO("RECV %zu", total_recv);
    memcpy(data, header.data, min(total_recv, size));

    while (total_recv < header.data_length) {
        LOG_INFO("while..");
        ssize_t recv_ret = PHYSICAL_LAYER__recv(socket->physical_layer, packet_buffer, MTU);
        if (recv_ret < 0) {
            LOG_ERROR("Failed to recv link layer data");
            return -1;
        }

        if (size > total_recv) {
            LOG_INFO("MMMM");
            memcpy(data + total_recv, packet_buffer, min(size-total_recv, recv_ret));
            LOG_INFO("LLLL");
        }

        total_recv += recv_ret;
    }

    return min(header.data_length, size);
}
