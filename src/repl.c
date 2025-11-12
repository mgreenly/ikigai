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

    // Initialize render
    result = ik_render_create(repl,
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

    // Initial render
    res_t result = ik_repl_render_frame(repl);
    if (is_err(&result)) {
        return result;
    }

    // Main event loop: read bytes, parse into actions, process, render
    char byte;
    while (!repl->quit) {
        ssize_t n = ik_read_wrapper(repl->term->tty_fd, &byte, 1);
        if (n <= 0) {
            // EOF or error - exit loop
            break;
        }

        // Parse byte into action
        ik_input_action_t action;
        ik_input_parse_byte(repl->input_parser, byte, &action);

        // Process action
        result = ik_repl_process_action(repl, &action);
        if (is_err(&result)) {
            return result;
        }

        // Render frame (only if action was not UNKNOWN)
        if (action.type != IK_INPUT_UNKNOWN) {
            result = ik_repl_render_frame(repl);
            if (is_err(&result)) {
                return result;
            }
        }
    }

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
    return ik_render_workspace(repl->render, text, text_len, cursor_byte_offset);
}

res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(action != NULL);   /* LCOV_EXCL_BR_LINE */

    if (action->type == IK_INPUT_CHAR) {
        return ik_workspace_insert_codepoint(repl->workspace, action->codepoint);
    } else if (action->type == IK_INPUT_NEWLINE) {
        return ik_workspace_insert_newline(repl->workspace);
    } else if (action->type == IK_INPUT_BACKSPACE) {
        return ik_workspace_backspace(repl->workspace);
    } else if (action->type == IK_INPUT_DELETE) {
        return ik_workspace_delete(repl->workspace);
    } else if (action->type == IK_INPUT_ARROW_LEFT) {
        return ik_workspace_cursor_left(repl->workspace);
    } else if (action->type == IK_INPUT_ARROW_RIGHT) {
        return ik_workspace_cursor_right(repl->workspace);
    } else if (action->type == IK_INPUT_ARROW_UP) {
        return ik_workspace_cursor_up(repl->workspace);
    } else if (action->type == IK_INPUT_ARROW_DOWN) {
        return ik_workspace_cursor_down(repl->workspace);
    } else if (action->type == IK_INPUT_CTRL_A) {
        return ik_workspace_cursor_to_line_start(repl->workspace);
    } else if (action->type == IK_INPUT_CTRL_E) {
        return ik_workspace_cursor_to_line_end(repl->workspace);
    } else if (action->type == IK_INPUT_CTRL_K) {
        return ik_workspace_kill_to_line_end(repl->workspace);
    } else if (action->type == IK_INPUT_CTRL_U) {
        return ik_workspace_kill_line(repl->workspace);
    } else if (action->type == IK_INPUT_CTRL_W) {
        return ik_workspace_delete_word_backward(repl->workspace);
    } else if (action->type == IK_INPUT_CTRL_C) {
        repl->quit = true;
    }

    return OK(NULL);
}
