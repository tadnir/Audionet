/**
 * Defines the transport layer of the audio socket.
 * The transport layer is responsible for retransmission and acknowledgment of packets.
 */

#ifndef AUDIONET_TRANSPORT_LAYER_H
#define AUDIONET_TRANSPORT_LAYER_H

#include <stdbool.h>
#include <sys/types.h>

/** The transport layer socket type. */
typedef struct audio_transport_layer_socket_s audio_transport_layer_socket_t;

/**
 * Allocates and initializes a new transport layer socket.
 *
 * @return The initialized socket, or NULL on failure.
 */
audio_transport_layer_socket_t* TRANSPORT_LAYER__initialize();

/**
 * Frees a transport layer socket.
 *
 * @param socket The socket to free.
 */
void TRANSPORT_LAYER__free(audio_transport_layer_socket_t *socket);

/**
 * Sends a packet over the transport layer socket,
 * success is returned only after acknowledgment is received.
 *
 * @param socket The socket to send data over.
 * @param data The data to send.
 * @param size The length of the data to send.
 * @return 0 on Success, -1 on Failure.
 */
int TRANSPORT_LAYER__send(audio_transport_layer_socket_t* socket, void* data, size_t size);

/**
 * Receives packet over the audio transport layer, sends an ack to the sender.
 *
 * @param socket The socket to receive the packet over.
 * @param data The buffer to save the incoming packet into.
 * @param size The size of the buffer.
 * @return The length of the packet on success,
 */
ssize_t TRANSPORT_LAYER__recv(audio_transport_layer_socket_t* socket, void* data, size_t size);

#endif //AUDIONET_TRANSPORT_LAYER_H
