#ifndef AUDIONET_ASOCKET_H
#define AUDIONET_ASOCKET_H

#include <stdbool.h>
#include <sys/types.h>

typedef struct asocket_s asocket_t;

asocket_t* ASOCKET__initialize();
void ASOCKET__free(asocket_t* socket);

int ASOCKET__listen(asocket_t* socket);

int ASOCKET__send(asocket_t* socket, void* data, size_t size);
ssize_t ASOCKET__recv(asocket_t* socket, void* data, size_t size);

#endif //AUDIONET_ASOCKET_H
