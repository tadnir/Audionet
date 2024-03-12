#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "utils/logger.h"
#include "audio_socket/audio_socket.h"


int main(int argc, char **argv) {
    int status;
    audio_socket_t* socket = AUDIO_SOCKET__initialize(AUDIO_LAYER_TRANSPORT);
    if (socket == NULL) {
        LOG_ERROR("Failed to initialize socket");
        status = -1;
        goto l_cleanup;
    }

    char buffer[1024];
    ssize_t recv_length = AUDIO_SOCKET__recv(socket, buffer, sizeof(buffer));
    if (recv_length < 0) {
        LOG_ERROR("Failed to recv message on socket: %zd", recv_length);
        status = -1;
        goto l_cleanup;
    }

    size_t length = strnlen(buffer, recv_length);
    if (length == sizeof(buffer)) {
        LOG_WARNING("No null terminator, adding at end");
        buffer[length - 1] = '\0';
    }

    LOG_INFO("Got: <%s> %zd", buffer, recv_length);

    LOG_INFO("Finished");
    status = 0;

l_cleanup:
    if (socket != NULL) {
        AUDIO_SOCKET__free(socket);
    }

    return status;
}