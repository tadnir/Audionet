#ifndef AUDIONET_PHYSICAL_LAYER_H
#define AUDIONET_PHYSICAL_LAYER_H

#include <stdbool.h>
#include <sys/types.h>

typedef struct audio_link_layer_socket_s audio_link_layer_socket_t;

audio_link_layer_socket_t* LINK_LAYER__initialize();
void LINK_LAYER__free(audio_link_layer_socket_t *socket);

int LINK_LAYER__listen(audio_link_layer_socket_t* socket);

int LINK_LAYER__send(audio_link_layer_socket_t* socket, void* data, size_t size);
ssize_t LINK_LAYER__recv(audio_link_layer_socket_t* socket, void* data, size_t size);

#endif //AUDIONET_PHYSICAL_LAYER_H
