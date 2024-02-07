#include <string.h>
#include "logger.h"
#include "audio_socket/audio_socket.h"


int main(int argc, char **argv) {
    int status;
    audio_socket_t* socket = AUDIO_SOCKET__initialize(AUDIO_LAYER_LINK);
    if (socket == NULL) {
        LOG_ERROR("Failed to initialize socket");
        status = -1;
        goto l_cleanup;
    }

    char* data = "Message";
    LOG_INFO("Sending: <%s>", data);
    status = AUDIO_SOCKET__send(socket, data, strlen(data) + 1);
    if (status != 0) {
        LOG_ERROR("Failed to send message on socket");
        goto l_cleanup;
    }

    LOG_INFO("Finished Sending");
    status = 0;

l_cleanup:
    if (socket != NULL) {
        AUDIO_SOCKET__free(socket);
    }

    return status;
}