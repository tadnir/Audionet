/**
 * Defines the physical layer of the audio socket.
 * The physical layer is responsible for low level encode/decode and send/recv of bytes over audio.
 */

#ifndef AUDIONET_PHYSICAL_LAYER_H
#define AUDIONET_PHYSICAL_LAYER_H

#include <stdbool.h>
#include <sys/types.h>

/**
 * Configures the packet size of a single audio packet.
 */
#define PHYSICAL_LAYER_MTU (9)

/**
 * The configured timeout until receive timeout failure.
 */
#define RECV_TIMEOUT_SECONDS (6)

/**
 * The error code for receive timeout.
 */
#define RECV_TIMEOUT_RET_CODE (-2)

/**
 * The physical layer socket type.
 */
typedef struct audio_physical_layer_socket_s audio_physical_layer_socket_t;

/**
 * Allocates and initializes a new physical layer socket.
 *
 * @return The initialized socket, or NULL on failure.
 */
audio_physical_layer_socket_t* PHYSICAL_LAYER__initialize();

/**
 * Frees a physical layer socket.
 *
 * @param socket The socket to free.
 */
void PHYSICAL_LAYER__free(audio_physical_layer_socket_t *socket);

/**
 * Sends a frame buffer over the physical layer socket, the size of the frame mustn't exceed `PHYSICAL_LAYER_MTU`.
 *
 * @param socket The socket over which to send the data.
 * @param frame The frame buffer to send.
 * @param size The size of the frame.
 * @return 0 On Success, -1 On Failure.
 */
int PHYSICAL_LAYER__send(audio_physical_layer_socket_t* socket, void* frame, size_t size);

/**
 * Waits to receive a frame buffer over the physical layer socket,
 * the size of the frame buffer must be at least `PHYSICAL_LAYER_MTU`.
 * After a timeout has expired, returns a dedicated error code - `RECV_TIMEOUT_RET_CODE`.
 *
 * @param socket The socket over which to receive data.
 * @param frame The buffer to save the incoming frame into.
 * @param size The size of the frame buffer.
 * @return The number of bytes in the read buffer on success, or negative code on error.
 */
ssize_t PHYSICAL_LAYER__recv(audio_physical_layer_socket_t* socket, void* frame, size_t size);

/**
 * Checks whether a frame has been recorded by the socket.
 * This call will allow the user to get the frame data without considering it as handled.
 * Can also be required to wait in a blocking manner for a frame to arrive upto a timeout of `RECV_TIMEOUT_SECONDS`.
 *
 * @param socket The socket to peek from.
 * @param frame The buffer to save the incoming frame into, optional.
 * @param size The size of the frame buffer.
 * @param blocking Whether the function should wait (up until timeout) for a frame to arrive.
 * @return If a frame exists returns it's size, otherwise returns zero. Upon error (or timeout) returns a negative number.
 */
ssize_t PHYSICAL_LAYER__peek(audio_physical_layer_socket_t* socket, void* frame, size_t size, bool blocking);

/**
 * If there's a recorded frame, removes it.
 *
 * @param socket The socket to pop from.
 * @return 0 if a frame was removed, -1 otherwise.
 */
int PHYSICAL_LAYER__pop(audio_physical_layer_socket_t* socket);

#endif //AUDIONET_PHYSICAL_LAYER_H
