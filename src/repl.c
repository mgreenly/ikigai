#include "repl.h"
#include "wrapper.h"
#include <assert.h>
#include <talloc.h>

res_t ik_repl_init(void *parent, ik_repl_ctx_t **repl_out)
{
    assert(parent != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(repl_out != NULL);   /* LCOV_EXCL_BR_LINE */

    // Allocate REPL context
    ik_repl_ctx_t *repl = ik_talloc_zero_wrapper(parent, sizeof(ik_repl_ctx_t));
    if (repl == NULL) {
        return ERR(parent, OOM, "Failed to allocate REPL context");
    }

    // Initialize terminal (raw mode + alternate screen)
    res_t result = ik_term_init(repl, &repl->term);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }

    // Initialize render_direct
    result = ik_render_direct_create(repl,
                                     repl->term->screen_rows,
                                     repl->term->screen_cols,
                                     repl->term->tty_fd,
                                     &repl->render);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }

    // Initialize workspace
    result = ik_workspace_create(repl, &repl->workspace);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }

    // Initialize input parser
    result = ik_input_parser_create(repl, &repl->input_parser);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }

    // Set quit flag to false
    repl->quit = false;

    *repl_out = repl;
    return OK(repl);
}

void ik_repl_cleanup(ik_repl_ctx_t *repl)
{
    if (repl == NULL) {
        return;
    }

    // Cleanup terminal (restore state)
    if (repl->term != NULL) {
        ik_term_cleanup(repl->term);
    }

    // Other components cleaned up via talloc hierarchy
    talloc_free(repl);
}

res_t ik_repl_run(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // TODO: Implement event loop
    return OK(NULL);
}

res_t ik_repl_render_frame(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->render != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->workspace != NULL);   /* LCOV_EXCL_BR_LINE */

    // Get workspace text (cannot fail - always returns OK)
    char *text = NULL;
    size_t text_len = 0;
    ik_workspace_get_text(repl->workspace, &text, &text_len);

    // Get cursor byte offset (cannot fail - always returns OK)
    size_t cursor_byte_offset = 0;
    size_t cursor_grapheme = 0;
    ik_workspace_get_cursor_position(repl->workspace, &cursor_byte_offset, &cursor_grapheme);

    // Render workspace with cursor
    return ik_render_direct_workspace(repl->render, text, text_len, cursor_byte_offset);
}
