#include "repl.h"
#include "repl_actions.h"
#include "signal_handler.h"
#include "panic.h"
#include "wrapper.h"
#include "format.h"
#include "input_buffer/core.h"
#include "render_cursor.h"
#include <assert.h>
#include <talloc.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

res_t ik_repl_init(void *parent, ik_repl_ctx_t **repl_out)
{
    assert(parent != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(repl_out != NULL);   /* LCOV_EXCL_BR_LINE */

    // Allocate REPL context
    ik_repl_ctx_t *repl = talloc_zero_(parent, sizeof(ik_repl_ctx_t));
    if (repl == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

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

    // Initialize input buffer
    result = ik_input_buffer_create(repl, &repl->input_buffer);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Initialize input parser
    result = ik_input_parser_create(repl, &repl->input_parser);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Initialize scrollback buffer (Phase 4)
    result = ik_scrollback_create(repl, repl->term->screen_cols, &repl->scrollback);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Initialize viewport offset to 0 (at bottom)
    repl->viewport_offset = 0;

    // Set quit flag to false
    repl->quit = false;

    // Initialize layer-based rendering (Phase 1.3)
    // Initialize reference fields
    repl->spinner_state.frame_index = 0;
    repl->spinner_state.visible = false;  // Initially hidden
    repl->separator_visible = true;
    repl->input_buffer_visible = true;
    repl->input_text = "";
    repl->input_text_len = 0;

    // Create layer cake
    result = ik_layer_cake_create(repl, (size_t)repl->term->screen_rows, &repl->layer_cake);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create scrollback layer
    result = ik_scrollback_layer_create(repl, "scrollback", repl->scrollback, &repl->scrollback_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create spinner layer (Phase 1.4)
    result = ik_spinner_layer_create(repl, "spinner", &repl->spinner_state, &repl->spinner_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create separator layer
    result = ik_separator_layer_create(repl, "separator", &repl->separator_visible, &repl->separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create input layer
    result = ik_input_layer_create(repl, "input", &repl->input_buffer_visible,
                                   &repl->input_text, &repl->input_text_len,
                                   &repl->input_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Add layers to cake (in order: scrollback, spinner, separator, input)
    result = ik_layer_cake_add_layer(repl->layer_cake, repl->scrollback_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->spinner_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->input_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Set up signal handlers (SIGWINCH for terminal resize)
    result = ik_signal_handler_init(parent);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - Signal handler setup failure is rare (invalid signal number)
        talloc_free(repl);  // LCOV_EXCL_LINE
        return result;  // LCOV_EXCL_LINE
    }

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
        // Check for pending resize before reading
        result = ik_signal_check_resize(repl);
        if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - Signal handler errors require actual signal delivery
            return result;
        }

        ssize_t n = posix_read_(repl->term->tty_fd, &byte, 1);
        if (n < 0) {
            // LCOV_EXCL_START - Signal interruption requires actual signal delivery
            // Check if interrupted by signal
            if (errno == EINTR) {
                // Signal interrupted read - check for resize and continue
                result = ik_signal_check_resize(repl);
                if (is_err(&result)) {
                    return result;
                }
                continue;  // Retry read
            }
            // LCOV_EXCL_STOP
            // Other error - exit loop
            break;
        }
        if (n == 0) {
            // EOF - exit loop
            break;
        }

        // Parse byte into action
        ik_input_action_t action;
        ik_input_parse_byte(repl->input_parser, byte, &action);

        // Process action
        result = ik_repl_process_action(repl, &action);
        if (is_err(&result)) return result; // LCOV_EXCL_LINE - Defensive check, input parser validates codepoints

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

res_t ik_repl_submit_line(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Get current input buffer text
    const char *text = (const char *)repl->input_buffer->text->data;
    size_t text_len = ik_byte_array_size(repl->input_buffer->text);

    // Append to scrollback (only if there's content and scrollback exists)
    if (text_len > 0 && repl->scrollback != NULL) {
        res_t result = ik_scrollback_append_line(repl->scrollback, text, text_len);
        if (is_err(&result)) return result; // LCOV_EXCL_LINE - Defensive, allocation failures are rare
    }

    // Clear input buffer
    ik_input_buffer_clear(repl->input_buffer);

    // Auto-scroll to bottom (reset viewport offset)
    repl->viewport_offset = 0;

    return OK(NULL);
}

res_t ik_repl_handle_resize(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->term != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->scrollback != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->input_buffer != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->render != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update terminal dimensions
    int rows, cols;
    res_t result = ik_term_get_size(repl->term, &rows, &cols);
    if (is_err(&result)) {
        return result;
    }

    // Update render context dimensions
    repl->render->rows = rows;
    repl->render->cols = cols;

    // Invalidate layout caches (will recalculate on next ensure_layout call)
    ik_scrollback_ensure_layout(repl->scrollback, cols);
    ik_input_buffer_ensure_layout(repl->input_buffer, cols);

    // Trigger immediate redraw with new dimensions
    return ik_repl_render_frame(repl);
}
