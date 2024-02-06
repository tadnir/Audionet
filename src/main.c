#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "logger.h"
#include "audio_socket/audio_socket.h"


int main(int argc, char **argv) {
    int status;
    audio_socket_t* socket = AUDIO_SOCKET__initialize(AUDIO_LAYER_PHYSICAL);
    if (socket == NULL) {
        LOG_ERROR("Failed to initialize socket");
        status = -1;
        goto l_cleanup;
    }

    LOG_INFO("Starting listen");
    status = AUDIO_SOCKET__listen(socket);
    if (status != 0) {
        LOG_ERROR("Failed to start listening");
        goto l_cleanup;
    }

    char* data = "Lorem";
    LOG_INFO("Sending: %s", data);
    status = AUDIO_SOCKET__send(socket, data, strlen(data));
    if (status != 0) {
        LOG_ERROR("Failed to send message on socket");
        goto l_cleanup;
    }

    char buffer[1024];
    ssize_t recv_length = AUDIO_SOCKET__recv(socket, buffer, sizeof(buffer));
    if (recv_length == -1) {
        LOG_ERROR("Failed to recv message on socket");
        status  = -1;
        goto l_cleanup;
    }

    buffer[1023] = '\0';
    LOG_INFO("Got: %s", buffer);

    LOG_INFO("Finished");
    status = 0;

l_cleanup:
    if (socket != NULL) {
        AUDIO_SOCKET__free(socket);
    }

    return status;
}