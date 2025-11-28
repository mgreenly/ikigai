#include <stdio.h>
#include <stdlib.h>
#include <talloc.h>

#include "repl.h"
#include "error.h"
#include "panic.h"

/* LCOV_EXCL_START */
int main(void)
{
    void *root_ctx = talloc_new(NULL);
    if (root_ctx == NULL)PANIC("Failed to create root talloc context");

    ik_repl_ctx_t *repl = NULL;
    res_t result = ik_repl_init(root_ctx, &repl);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    // Set global for panic handler (after terminal initialization)
    g_term_ctx_for_panic = repl->term;

    // Hook talloc abort handler
    talloc_set_abort_fn(ik_talloc_abort_handler);

    result = ik_repl_run(repl);

    ik_repl_cleanup(repl);
    talloc_free(root_ctx);

    return is_ok(&result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* LCOV_EXCL_STOP */
