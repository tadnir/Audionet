#ifndef AUDIONET_AUDIO_SOCKET_H
#define AUDIONET_AUDIO_SOCKET_H

#include <stdbool.h>
#include <sys/types.h>

enum audio_socket_layer {
    AUDIO_LAYER_PHYSICAL,
    AUDIO_LAYER_LINK,
    AUDIO_LAYER_TRANSPORT,
};

typedef struct audio_socket_s audio_socket_t;

audio_socket_t* AUDIO_SOCKET__initialize(enum audio_socket_layer layer);
void AUDIO_SOCKET__free(audio_socket_t *socket);

int AUDIO_SOCKET__send(audio_socket_t* socket, void* data, size_t size);
ssize_t AUDIO_SOCKET__recv(audio_socket_t* socket, void* data, size_t size);

#endif //AUDIONET_AUDIO_SOCKET_H
