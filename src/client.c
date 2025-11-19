#include "config.h"
#include "error.h"
#include "panic.h"
#include "repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <talloc.h>

/* LCOV_EXCL_START */
int main(void)
{
    void *root_ctx = talloc_new(NULL);
    if (root_ctx == NULL) PANIC("Failed to create root talloc context");

    // Load configuration
    res_t cfg_result = ik_cfg_load(root_ctx, "~/.config/ikigai/config.json");
    if (is_err(&cfg_result)) {
        error_fprintf(stderr, cfg_result.err);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }
    ik_cfg_t *cfg = cfg_result.ok;

    ik_repl_ctx_t *repl = NULL;
    res_t result = ik_repl_init(root_ctx, cfg, &repl);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }
    // Our abort implementation uses `g_term_ctx_for_panic` to restore the primary buffer if it's not NULL.
    g_term_ctx_for_panic = repl->term;
    // The talloc library will call this, instead of `abort` if it's defined, which will restore the primary buffer.
    talloc_set_abort_fn(ik_talloc_abort_handler);

    result = ik_repl_run(repl);

    ik_repl_cleanup(repl);
    talloc_free(root_ctx);

    return is_ok(&result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* LCOV_EXCL_STOP */
