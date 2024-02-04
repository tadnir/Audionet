#include "logger.h"
#include "asocket/asocket.h"


int main(int argc, char **argv) {
    int status;
    asocket_t* socket = ASOCKET__initialize();
    if (socket == NULL) {
        LOG_ERROR("Failed to initialize socket");
        status = -1;
        goto l_cleanup;
    }

    LOG_INFO("Starting listen");
    status = ASOCKET__listen(socket);
    if (status != 0) {
        LOG_ERROR("Failed to start listening");
        goto l_cleanup;
    }

    LOG_INFO("Sending");
    status = ASOCKET__send(socket, "Hello, World!", 14);
    if (status != 0) {
        LOG_ERROR("Failed to send message on socket");
        goto l_cleanup;
    }

    char buffer[1024];
    ssize_t recv_length = ASOCKET__recv(socket, buffer, sizeof(buffer));
    if (recv_length == -1) {
        LOG_ERROR("Failed to recv message on socket");
        status  = -1;
        goto l_cleanup;
    }

    LOG_INFO("Finished");
    status = 0;

l_cleanup:
    if (socket != NULL) {
        ASOCKET__free(socket);
    }

    return status;
}