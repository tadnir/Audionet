#include <malloc.h>

#include "utils/logger.h"
#include "audio_socket.h"
#include "audio_socket/layers/physical/physical_layer.h"
#include "audio_socket/layers/link/link_layer.h"
#include "audio_socket/layers/transport/transport_layer.h"

/**
 * Defines the list of the different layers possible for the audio socket.
 * We can later decide which layer we want to use for debugging purposes.
 */
enum audio_socket_layer {
    AUDIO_LAYER_PHYSICAL,
    AUDIO_LAYER_LINK,
    AUDIO_LAYER_TRANSPORT,
};

/**
 * Here we can define the socket layer that will be used, useful for debugging lower layers.
 */
#define SOCKET_LAYER (AUDIO_LAYER_TRANSPORT)

struct audio_socket_s {
    /** The layer at which the socket operates */
    enum audio_socket_layer layer;

    /** If `layer` is physical this implementation will be used */
    audio_physical_layer_socket_t* physical_layer;

    /** If `layer` is link this implementation will be used */
    audio_link_layer_socket_t * link_layer;

    /** If `layer` is transport this implementation will be used */
    audio_transport_layer_socket_t * transport_layer;
};

audio_socket_t *AUDIO_SOCKET__initialize() {
    /* Allocate the socket object */
    audio_socket_t* socket = malloc(sizeof(audio_socket_t));
    if (socket == NULL) {
        LOG_ERROR("Failed to allocate audio socket");
        return NULL;
    }

    /* Initialize the chosen layer, the rest will be uninitialized */
    socket->layer = SOCKET_LAYER;
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
        case AUDIO_LAYER_TRANSPORT:
            socket->transport_layer = TRANSPORT_LAYER__initialize();
            if (socket->transport_layer == NULL) {
                free(socket);
                LOG_ERROR("Failed to initialize audio socket transport layer");
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

    /* Free the initialized layers */
    if (socket->physical_layer != NULL) {
        PHYSICAL_LAYER__free(socket->physical_layer);
        socket->physical_layer = NULL;
    }
    if (socket->link_layer != NULL) {
        LINK_LAYER__free(socket->link_layer);
        socket->link_layer = NULL;
    }
    if (socket->transport_layer != NULL) {
        TRANSPORT_LAYER__free(socket->transport_layer);
        socket->transport_layer = NULL;
    }

    /* Free the socket object */
    free(socket);
}

int AUDIO_SOCKET__send(audio_socket_t *socket, void *data, size_t size) {
    /* Send the data buffer using the appropriate layer.
     * The lower layers have size limitations, since the lower level sockets are for debugging purposes
     * it is left to the user to validate the size of the given buffer is not too big.*/
    switch (socket->layer) {
        case AUDIO_LAYER_PHYSICAL:
            return PHYSICAL_LAYER__send(socket->physical_layer, data, size);
        case AUDIO_LAYER_LINK:
            return LINK_LAYER__send(socket->link_layer, data, size);
        case AUDIO_LAYER_TRANSPORT:
            return TRANSPORT_LAYER__send(socket->transport_layer, data, size);
    }
}

ssize_t AUDIO_SOCKET__recv(audio_socket_t *socket, void *data, size_t size) {
    /* Receive data using the appropriate socket layer.  */
    switch (socket->layer) {
        case AUDIO_LAYER_PHYSICAL:
            return PHYSICAL_LAYER__recv(socket->physical_layer, data, size);
        case AUDIO_LAYER_LINK:
            return LINK_LAYER__recv(socket->link_layer, data, size);
        case AUDIO_LAYER_TRANSPORT:
            return TRANSPORT_LAYER__recv(socket->transport_layer, data, size);
    }
}
