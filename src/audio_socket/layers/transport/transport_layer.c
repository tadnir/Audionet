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
} __attribute__((packed));

struct transport_packet_s {
    struct transport_packet_header_s header;
    uint8_t data[LINK_LAYER_MTU - sizeof(struct transport_packet_header_s)];
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
    size_t data_remaining = size + sizeof(uint32_t);
    uint8_t* current_data_ptr = data;
    struct transport_packet_s packet_out;
    struct transport_packet_s packet_in;
    size_t data_sending;
    size_t size_header_fix = sizeof(uint32_t);

    packet_out.header.seq = socket->seq;
    data_sending = min(data_remaining, sizeof(packet_out.data));
    *((uint32_t*)packet_out.data) = size;
    memcpy(packet_out.data + sizeof(uint32_t), current_data_ptr, data_sending - sizeof(uint32_t));
    while (data_remaining > 0) {
        ret = LINK_LAYER__send(socket->link_layer, &packet_out, sizeof(packet_out.header) + data_sending);
        if (ret != 0) {
            LOG_ERROR("Failed to send on link layer");
            return -1;
        }

        recv_ret = LINK_LAYER__recv(socket->link_layer, &packet_in, sizeof(packet_in));
        if (recv_ret == RECV_TIMEOUT_RET_CODE) {
            LOG_INFO("Timed out, retrying send");
            continue;
        } else if (recv_ret == RECV_OUT_OF_SYNC_RET_CODE) {
            LOG_INFO("Out of sync");
            continue;
        } else if (recv_ret < 0) {
            LOG_ERROR("Failed to recv ack on transport layer");
            return -1;
        }

        if (packet_in.header.seq == packet_out.header.seq) {
            socket->seq++;
            data_remaining -= data_sending;
            current_data_ptr += data_sending - size_header_fix;
            size_header_fix = 0;
            data_sending = min(data_remaining, sizeof(packet_out.data));
            packet_out.header.seq = socket->seq;
            memcpy(packet_out.data, current_data_ptr, data_sending);
        }
    }

    return 0;
}

ssize_t TRANSPORT_LAYER__recv(audio_transport_layer_socket_t *socket, void *data, size_t size) {
    int ret = -1;
    ssize_t recv_ret = -1;
    struct transport_packet_header_s packet_out;
    struct transport_packet_s packet_in;
    size_t index = 0;
    uint32_t data_length = 0;
    bool read_data_length = false;

    while (index < size) {
        recv_ret = LINK_LAYER__recv(socket->link_layer, &packet_in, sizeof(packet_in));
        if (recv_ret == RECV_TIMEOUT_RET_CODE) {
            LOG_WARNING("Timed out on transport recv");
            continue;
        } else if (recv_ret == RECV_OUT_OF_SYNC_RET_CODE) {
            LOG_INFO("Out of sync");
            continue;
        } else if (recv_ret < 0) {
            LOG_ERROR("Failed to recv on transport layer: %zd", recv_ret);
            return recv_ret;
        }

        if (packet_in.header.seq < socket->seq) {
            socket->seq--;
        } else if (packet_in.header.seq > socket->seq) {
            LOG_WARNING("Bad seq %d", packet_in.header.seq);
            return -1;
        } else {
            if (!read_data_length) {
                data_length = *(uint32_t*)packet_in.data;
                read_data_length = true;
                memcpy(((uint8_t*)data) + index, packet_in.data + sizeof(uint32_t), min(recv_ret-sizeof(uint32_t), size - index));
                index += min(recv_ret - sizeof(uint32_t), size - index);
                LOG_DEBUG("transport layer recv packet size %d", data_length);
            } else {
                memcpy(((uint8_t*)data) + index, packet_in.data, min(recv_ret, size - index));
                index += min(recv_ret, size - index);
            }
        }

        packet_out.seq = packet_in.header.seq;
        socket->seq++;
        ret = LINK_LAYER__send(socket->link_layer, &packet_out, sizeof(packet_out));
        if (ret != 0) {
            LOG_ERROR("Failed to send transport layer ack");
            return -1;
        }

        if (read_data_length && index >= data_length) {
            break;
        }
    }

    return (ssize_t)index;
}
