#ifndef AUDIONET_TRANSPORT_LAYER_H
#define AUDIONET_TRANSPORT_LAYER_H

#include <stdbool.h>
#include <sys/types.h>

typedef struct audio_transport_layer_socket_s audio_transport_layer_socket_t;

audio_transport_layer_socket_t* TRANSPORT_LAYER__initialize();
void TRANSPORT_LAYER__free(audio_transport_layer_socket_t *socket);

int TRANSPORT_LAYER__send(audio_transport_layer_socket_t* socket, void* data, size_t size);
ssize_t TRANSPORT_LAYER__recv(audio_transport_layer_socket_t* socket, void* data, size_t size);

#endif //AUDIONET_TRANSPORT_LAYER_H
