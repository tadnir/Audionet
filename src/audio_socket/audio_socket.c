#include <malloc.h>

#include "logger.h"
#include "audio_socket.h"
#include "audio_socket/layers/physical/physical_layer.h"

struct audio_socket_s {
    audio_physical_layer_socket_t* physical_layer;
};

audio_socket_t *AUDIO_SOCKET__initialize(enum audio_socket_layer layer) {
    audio_socket_t* socket = malloc(sizeof(audio_socket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to allocate audio socket");
        return NULL;
    }

    socket->physical_layer = PHYSICAL_LAYER__initialize();
    if (socket->physical_layer == NULL) {
        free(socket);
        LOG_ERROR("Failed to initialize audio socket physical layer");
        return NULL;
    }

    return socket;
}

void AUDIO_SOCKET__free(audio_socket_t *socket) {
    if (socket == NULL) {
        LOG_WARNING("Cannot free NULL audio socket");
        return;
    }

    PHYSICAL_LAYER__free(socket->physical_layer);
    free(socket);
}

int AUDIO_SOCKET__listen(audio_socket_t *socket) {
    return PHYSICAL_LAYER__listen(socket->physical_layer);
}

int AUDIO_SOCKET__send(audio_socket_t *socket, void *data, size_t size) {
    return PHYSICAL_LAYER__send(socket->physical_layer, data, size);
}

ssize_t AUDIO_SOCKET__recv(audio_socket_t *socket, void *data, size_t size) {
    return PHYSICAL_LAYER__recv(socket->physical_layer, data, size);
}
