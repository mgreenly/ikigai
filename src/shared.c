#include "shared.h"

#include "panic.h"
#include "wrapper.h"
#include "terminal.h"
#include "render.h"

#include <assert.h>

// Destructor for shared context - handles cleanup
static int shared_destructor(ik_shared_ctx_t *shared)
{
    // Cleanup terminal (restore state)
    if (shared->term != NULL) {
        ik_term_cleanup(shared->term);
    }
    return 0;
}

res_t ik_shared_ctx_init(TALLOC_CTX *ctx, ik_cfg_t *cfg, ik_shared_ctx_t **out)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(cfg != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    ik_shared_ctx_t *shared = talloc_zero_(ctx, sizeof(ik_shared_ctx_t));
    if (shared == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    shared->cfg = cfg;

    // Initialize terminal (raw mode + alternate screen)
    res_t result = ik_term_init(shared, &shared->term);
    if (is_err(&result)) {
        talloc_free(shared);
        return result;
    }

    // Initialize render
    result = ik_render_create(shared,
                              shared->term->screen_rows,
                              shared->term->screen_cols,
                              shared->term->tty_fd,
                              &shared->render);
    if (is_err(&result)) {
        ik_term_cleanup(shared->term);
        talloc_free(shared);
        return result;
    }

    // Set destructor for cleanup
    talloc_set_destructor(shared, shared_destructor);

    *out = shared;
    return OK(shared);
}
