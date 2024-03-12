#include <string.h>
#include "utils/logger.h"
#include "audio_socket/audio_socket.h"

/** The usage string of the program */
#define USAGE "AudioClient <message>"

/**
 * Main function for the client example program.
 * Sends the given message to the server.
 *
 * @param argc The number of arguments to the program, expected value 2.
 * @param argv The arguments to the program (including the program name), arg index 1 is the text to send.
 * @return 0 On Success, -1 On Failure.
 */
int main(int argc, char **argv) {
    int status;

    /* Validate the number of arguments is as expected. */
    if (argc != 2) {
        printf(USAGE "\n");
        return -1;
    }

    /* Get the input message string. */
    char* data = argv[1];
    size_t data_length = strlen(data) + 1;

    /* Initialize the client socket. */
    audio_socket_t* socket = AUDIO_SOCKET__initialize();
    if (socket == NULL) {
        LOG_ERROR("Failed to initialize socket");
        status = -1;
        goto l_cleanup;
    }

    /* Send the message to the server. */
    LOG_INFO("Sending: <%s>", data);
    status = AUDIO_SOCKET__send(socket, data, data_length);
    if (status != 0) {
        LOG_ERROR("Failed to send message on socket");
        goto l_cleanup;
    }

    LOG_INFO("Finished Sending");
    status = 0;

l_cleanup:
    /* Free the audio socket */
    if (socket != NULL) {
        AUDIO_SOCKET__free(socket);
    }

    return status;
}