#include "repl.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <talloc.h>

/* LCOV_EXCL_START */
int main(void)
{
    void *root_ctx = talloc_new(NULL);
    if (!root_ctx) {
        fprintf(stderr, "Failed to create talloc context\n");
        return EXIT_FAILURE;
    }

    ik_repl_ctx_t *repl = NULL;
    res_t result = ik_repl_init(root_ctx, &repl);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    result = ik_repl_run(repl);

    ik_repl_cleanup(repl);
    talloc_free(root_ctx);

    return is_ok(&result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* LCOV_EXCL_STOP */
