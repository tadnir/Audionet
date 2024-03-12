#ifndef AUDIONET_AUDIO_SOCKET_H
#define AUDIONET_AUDIO_SOCKET_H

#include <stdbool.h>
#include <sys/types.h>

/**
 * The audio socket type.
 */
typedef struct audio_socket_s audio_socket_t;

/**
 * Initializes a new audio socket.
 *
 * @return The initialized audio socket.
 */
audio_socket_t* AUDIO_SOCKET__initialize();

/**
 * Frees an audio socket.
 *
 * @param socket The audio socket to free.
 */
void AUDIO_SOCKET__free(audio_socket_t *socket);

/**
 * Sends a buffer over the audio socket.
 *
 * @param socket The socket.
 * @param data The data to be sent.
 * @param size The length of the data buffer.
 * @return 0 On Success, -1 On Failure.
 */
int AUDIO_SOCKET__send(audio_socket_t* socket, void* data, size_t size);

/**
 * Blockly waits for incoming data on the socket.
 *
 * @param socket The socket.
 * @param data Returns the data received.
 * @param size The size of the data buffer.
 * @return -1 on Failure, the amount of bytes received on success.
 */
ssize_t AUDIO_SOCKET__recv(audio_socket_t* socket, void* data, size_t size);

#endif //AUDIONET_AUDIO_SOCKET_H
