#include <stdio.h>

#include "audio/audio.h"


int main(int argc, char **argv) {
    int ret = -1;
    printf("Started..\n");
    ret = poc();
    if (ret != 0) {
        printf("Failure %d\n", ret);
        goto l_cleanup;
    }

    printf("Finished.\n");
    ret = 0;
l_cleanup:

    return ret;
}