/**
 * Defines the link layer of the audio socket.
 * The link layer is responsible for enabling large size packets and packet ordering.
 */

#ifndef AUDIONET_LINK_LAYER_H
#define AUDIONET_LINK_LAYER_H

#include <stdbool.h>
#include <sys/types.h>
#include "audio_socket/layers/physical/physical_layer.h"

/** The maximum length that can be transmitted in a single link packet. */
#define LINK_LAYER_MTU (256 * PHYSICAL_LAYER_MTU)

/** The return code for recv operation timeout. */
#define RECV_TIMEOUT_RET_CODE (-2)

/** The return code for recv operation out-of-sync error. */
#define RECV_OUT_OF_SYNC_RET_CODE (-3)

/**
 * The link layer socket type.
 */
typedef struct audio_link_layer_socket_s audio_link_layer_socket_t;

/**
 * Allocates and initializes a new link layer socket.
 *
 * @return The initialize socket, or NULL on failure.
 */
audio_link_layer_socket_t* LINK_LAYER__initialize();

/**
 * Frees a link layer socket.
 *
 * @param socket The socket to free.
 */
void LINK_LAYER__free(audio_link_layer_socket_t *socket);

/**
 * Sends a packet over the link layer socket, the size of the packet mustn't exceed `LINK_LAYER_MTU`.
 *
 * @param socket The socket to send data over.
 * @param data The data to send.
 * @param size the length of the data to send.
 * @return 0 On Success, -1 On Failure.
 */
int LINK_LAYER__send(audio_link_layer_socket_t* socket, void* data, size_t size);

/**
 * Receives packet over the audio link layer.
 * May fail on timeout or synchronization with sender.
 *
 * @param socket The socket to receive the packet over.
 * @param data The buffer to save the incoming packet into.
 * @param size The size of the buffer.
 * @return The length of the packet on success, negative value on failure.
 */
ssize_t LINK_LAYER__recv(audio_link_layer_socket_t* socket, void* data, size_t size);

#endif //AUDIONET_LINK_LAYER_H
