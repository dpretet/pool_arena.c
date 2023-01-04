// distributed under the mit license
// https://opensource.org/licenses/mit-license.php

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "malloc.h"

int main(/*int argc, char *argv[]*/) {

    // Pool arena size to manage
    int size = 1024 * sizeof(uint32_t);
    // Get first a space in system memory
    void * heap = malloc(size);
    // printf("Arena address: %p\n", heap);

    // Init malloc
    heap_init(heap, size);

    // Start testing the arena
    void * blks[16];
    blks[0] = _malloc(4);
    blks[1] = _malloc(4);
    blks[2] = _malloc(4);
    blks[3] = _malloc(4);

    // for (int i=0;i<4;i++)
        // printf("addr[%d] = %p\n", i, blks[i]);

    return 0;
}

