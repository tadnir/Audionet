#ifndef AUDIONET_PHYSICAL_LAYER_H
#define AUDIONET_PHYSICAL_LAYER_H

#include <stdbool.h>
#include <sys/types.h>

#define MTU (8)
#define RECV_TIMEOUT_SECONDS (10)
#define RECV_TIMEOUT_RET_CODE (-2)

typedef struct audio_physical_layer_socket_s audio_physical_layer_socket_t;

audio_physical_layer_socket_t* PHYSICAL_LAYER__initialize();
void PHYSICAL_LAYER__free(audio_physical_layer_socket_t *socket);

int PHYSICAL_LAYER__send(audio_physical_layer_socket_t* socket, void* frame, size_t size);
ssize_t PHYSICAL_LAYER__recv(audio_physical_layer_socket_t* socket, void* frame, size_t size);

#endif //AUDIONET_PHYSICAL_LAYER_H
