#include <malloc.h>

#include "logger.h"
#include "audio_socket.h"
#include "audio_socket/layers/physical/physical_layer.h"
#include "audio_socket/layers/link/link_layer.h"

struct audio_socket_s {
    enum audio_socket_layer layer;
    audio_physical_layer_socket_t* physical_layer;
    audio_link_layer_socket_t * link_layer;
};

audio_socket_t *AUDIO_SOCKET__initialize(enum audio_socket_layer layer) {
    audio_socket_t* socket = malloc(sizeof(audio_socket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to allocate audio socket");
        return NULL;
    }

    socket->layer = layer;
    switch (socket->layer) {
        case AUDIO_LAYER_PHYSICAL:
            socket->physical_layer = PHYSICAL_LAYER__initialize();
            if (socket->physical_layer == NULL) {
                free(socket);
                LOG_ERROR("Failed to initialize audio socket physical layer");
                return NULL;
            }
            break;
        case AUDIO_LAYER_LINK:
            socket->link_layer = LINK_LAYER__initialize();
            if (socket->link_layer == NULL) {
                free(socket);
                LOG_ERROR("Failed to initialize audio socket link layer");
                return NULL;
            }
            break;
    }

    return socket;
}

void AUDIO_SOCKET__free(audio_socket_t *socket) {
    if (socket == NULL) {
        LOG_WARNING("Cannot free NULL audio socket");
        return;
    }

    if (socket->physical_layer != NULL) {
        PHYSICAL_LAYER__free(socket->physical_layer);
    }
    if (socket->link_layer != NULL) {
        LINK_LAYER__free(socket->link_layer);
    }
    free(socket);
}

int AUDIO_SOCKET__listen(audio_socket_t *socket) {
    switch (socket->layer) {
        case AUDIO_LAYER_PHYSICAL:
            return PHYSICAL_LAYER__listen(socket->physical_layer);
        case AUDIO_LAYER_LINK:
            return LINK_LAYER__listen(socket->link_layer);
    }
}

int AUDIO_SOCKET__send(audio_socket_t *socket, void *data, size_t size) {
    switch (socket->layer) {
        case AUDIO_LAYER_PHYSICAL:
            return PHYSICAL_LAYER__send(socket->physical_layer, data, size);
        case AUDIO_LAYER_LINK:
            return LINK_LAYER__send(socket->link_layer, data, size);
    }
}

ssize_t AUDIO_SOCKET__recv(audio_socket_t *socket, void *data, size_t size) {
    switch (socket->layer) {
        case AUDIO_LAYER_PHYSICAL:
            return PHYSICAL_LAYER__recv(socket->physical_layer, data, size);
        case AUDIO_LAYER_LINK:
            return LINK_LAYER__recv(socket->link_layer, data, size);
    }
}
