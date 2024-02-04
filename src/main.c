#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
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
    char* p = "Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries";
    status = ASOCKET__send(socket, p, strlen(p));
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

    LOG_DEBUG("got %zd", recv_length);
    int fd = open("out.txt", O_CREAT | O_TRUNC|O_RDWR, S_IRWXU);
    int out = write(fd, buffer, recv_length);
    LOG_DEBUG("write: %d, %s", out, out == -1 ? strerror(errno) : "");
    close(fd);

    LOG_INFO("Finished");
    status = 0;

l_cleanup:
    if (socket != NULL) {
        ASOCKET__free(socket);
    }

    return status;
}