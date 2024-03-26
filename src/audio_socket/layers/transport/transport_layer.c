#include <stdint.h>
#include <malloc.h>
#include <string.h>

#include "transport_layer.h"
#include "audio_socket/layers/link/link_layer.h"
#include "utils/logger.h"
#include "utils/utils.h"


struct audio_transport_layer_socket_s {
    /** The transport layer uses the link layer to send packets. */
    audio_link_layer_socket_t* link_layer;

    /** The current expected sequence number. */
    uint8_t seq;
};

/**
 * The header for each transport packet.
 */
struct transport_packet_header_s {
    /** The sequence number of the packet. */
    uint8_t seq;
} __attribute__((packed));

/**
 * The transport layer packet structure.
 */
struct transport_packet_s {
    /** The packet header */
    struct transport_packet_header_s header;

    /** The data carried by each packet */
    uint8_t data[LINK_LAYER_MTU - sizeof(struct transport_packet_header_s)];
} __attribute__((packed));

audio_transport_layer_socket_t *TRANSPORT_LAYER__initialize() {
    /* Allocate the transport layer socket */
    audio_transport_layer_socket_t* socket = malloc(sizeof(audio_transport_layer_socket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to allocate audio transport socket");
        return NULL;
    }

    /* Initialize the under laying link layer */
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
    /* Free the link layer */
    LINK_LAYER__free(socket->link_layer);

    /* Free the transport layer */
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

    /* Fill the first packet with the length of the data as uint32_t and the remaining space with data */
    packet_out.header.seq = socket->seq;
    data_sending = min(data_remaining, sizeof(packet_out.data));
    *((uint32_t*)packet_out.data) = size;
    memcpy(packet_out.data + sizeof(uint32_t), current_data_ptr, data_sending - sizeof(uint32_t));

    /* While there's data to send, send it and wait for ack */
    while (data_remaining > 0) {
        /* Send the current packet */
        ret = LINK_LAYER__send(socket->link_layer, &packet_out, sizeof(packet_out.header) + data_sending);
        if (ret != 0) {
            LOG_ERROR("Failed to send on link layer");
            return -1;
        }

        /* Try to receive an ack */
        recv_ret = LINK_LAYER__recv(socket->link_layer, &packet_in, sizeof(packet_in));
        if (recv_ret == RECV_TIMEOUT_RET_CODE) {
            /* Timeout - Retransmit */
            LOG_INFO("Timed out, retrying send");
            continue;
        } else if (recv_ret == RECV_OUT_OF_SYNC_RET_CODE) {
            /* Out-of-sync - Retransmit */
            LOG_INFO("Out of sync");
            continue;
        } else if (recv_ret < 0) {
            LOG_ERROR("Failed to recv ack on transport layer");
            return -1;
        }

        /* We managed to send the ack a packet, set the next one */
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
        /* Try to receive a packet */
        recv_ret = LINK_LAYER__recv(socket->link_layer, &packet_in, sizeof(packet_in));
        if (recv_ret == RECV_TIMEOUT_RET_CODE) {
            /* Timeout - retry */
            LOG_WARNING("Timed out on transport recv");
            continue;
        } else if (recv_ret == RECV_OUT_OF_SYNC_RET_CODE) {
            /* Out-of-sync - retry */
            LOG_INFO("Out of sync");
            continue;
        } else if (recv_ret < 0) {
            LOG_ERROR("Failed to recv on transport layer: %zd", recv_ret);
            return recv_ret;
        }

        if (packet_in.header.seq < socket->seq) {
            /* Drop the sequence down to re-ack */
            socket->seq--;
        } else if (packet_in.header.seq > socket->seq) {
            /* Somehow the sequence number has advanced too much, cannot recover from this easily */
            LOG_WARNING("Bad seq %d", packet_in.header.seq);
            return -1;
        } else {
            if (!read_data_length) {
                /* We havn't read the length of the data yet (from the first packet), read it first then any data */
                data_length = *(uint32_t*)packet_in.data;
                read_data_length = true;
                memcpy(((uint8_t*)data) + index, packet_in.data + sizeof(uint32_t), min(recv_ret-sizeof(uint32_t), size - index));
                index += min(recv_ret - sizeof(uint32_t), size - index);
                LOG_DEBUG("transport layer recv packet size %d", data_length);
            } else {
                /* Read incoming data to the buffer */
                memcpy(((uint8_t*)data) + index, packet_in.data, min(recv_ret, size - index));
                index += min(recv_ret, size - index);
            }
        }

        /* Ack the incoming packet */
        packet_out.seq = packet_in.header.seq;
        socket->seq++;
        ret = LINK_LAYER__send(socket->link_layer, &packet_out, sizeof(packet_out));
        if (ret != 0) {
            LOG_ERROR("Failed to send transport layer ack");
            return -1;
        }

        /* Finished when we received all the data expected from the data_length */
        if (read_data_length && index >= data_length) {
            break;
        }
    }

    return (ssize_t)index;
}
