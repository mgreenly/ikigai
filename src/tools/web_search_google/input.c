#include "input.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <talloc.h>

#define CK(x) if((x)==NULL)exit(1)

char *read_stdin_input(void *ctx, size_t *out_len)
{
    size_t bsz = 2048;
    size_t total = 0;
    char *in = talloc_array(ctx, char, (unsigned int)bsz);
    CK(in);  // LCOV_EXCL_BR_LINE

    size_t nr;
    while ((nr = fread(in + total, 1, bsz - total, stdin)) > 0) {
        total += nr;

        if (total >= bsz) {
            bsz *= 2;
            in = talloc_realloc(ctx, in, char, (unsigned int)bsz);
            CK(in);  // LCOV_EXCL_BR_LINE
        }
    }

    in[total] = '\0';

    if (out_len != NULL) {  // LCOV_EXCL_BR_LINE
        *out_len = total;
    }

    return in;
}
