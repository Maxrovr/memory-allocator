#include <stdio.h>
#include "sfmm.h"

#define PAYLOAD_TO_BLOCK(pp)                                                                         \
    ((sf_block *) ((char *) (pp) - (char *) &(((sf_block *) 0x0)->body.payload)))

int main(int argc, char const *argv[]) {


    size_t sz = 1;
    void * x = sf_malloc(sz);
    PAYLOAD_TO_BLOCK(x)->header &= ~PREV_BLOCK_ALLOCATED;
    sf_free(x);
    return EXIT_SUCCESS;
}
