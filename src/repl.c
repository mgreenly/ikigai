#include "repl.h"
#include "repl_actions.h"
#include "signal_handler.h"
#include "panic.h"
#include "wrapper.h"
#include "format.h"
#include "input_buffer.h"
#include <assert.h>
#include <talloc.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

res_t ik_repl_init(void *parent, ik_repl_ctx_t **repl_out)
{
    assert(parent != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(repl_out != NULL);   /* LCOV_EXCL_BR_LINE */

    // Allocate REPL context
    ik_repl_ctx_t *repl = ik_talloc_zero_wrapper(parent, sizeof(ik_repl_ctx_t));
    if (repl == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

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
    if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE

    // Initialize input parser
    result = ik_input_parser_create(repl, &repl->input_parser);
    if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE

    // Initialize scrollback buffer (Phase 4)
    result = ik_scrollback_create(repl, repl->term->screen_cols, &repl->scrollback);
    if (is_err(&result))PANIC("allocation failed");  /* LCOV_EXCL_BR_LINE */

    // Initialize viewport offset to 0 (at bottom)
    repl->viewport_offset = 0;

    // Set quit flag to false
    repl->quit = false;

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

        ssize_t n = ik_read_wrapper(repl->term->tty_fd, &byte, 1);
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
        if (is_err(&result))return result;  // LCOV_EXCL_LINE - Defensive check, input parser validates codepoints

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

res_t ik_repl_calculate_viewport(ik_repl_ctx_t *repl, ik_viewport_t *viewport_out)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(viewport_out != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->term != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->input_buffer != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->scrollback != NULL);   /* LCOV_EXCL_BR_LINE */

    // Ensure input buffer layout is up to date
    ik_input_buffer_ensure_layout(repl->input_buffer, repl->term->screen_cols);

    // Ensure scrollback layout is up to date
    ik_scrollback_ensure_layout(repl->scrollback, repl->term->screen_cols);

    // Get component sizes
    size_t input_buffer_rows = ik_input_buffer_get_physical_lines(repl->input_buffer);
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(repl->scrollback);
    size_t scrollback_line_count = ik_scrollback_get_line_count(repl->scrollback);
    int32_t terminal_rows = repl->term->screen_rows;

    // Unified document model:
    // Document = scrollback_rows + 1 (separator) + MAX(input_buffer_rows, 1)
    // Input buffer always occupies at least 1 row (for cursor visibility when empty)
    size_t input_buffer_display_rows = (input_buffer_rows == 0) ? 1 : input_buffer_rows;
    size_t separator_row = scrollback_rows;  // Separator is at this document row (0-indexed)
    size_t input_buffer_start_doc_row = scrollback_rows + 1;  // Input buffer starts here
    size_t document_height = scrollback_rows + 1 + input_buffer_display_rows;

    // Calculate visible document range
    // viewport_offset = how many rows scrolled UP from bottom
    size_t first_visible_row, last_visible_row;

    if (document_height <= (size_t)terminal_rows) {
        // Entire document fits on screen
        first_visible_row = 0;
        last_visible_row = document_height > 0 ? document_height - 1 : 0;  /* LCOV_EXCL_BR_LINE */
    } else {
        // Document overflows - calculate window
        // Clamp viewport_offset to valid range
        size_t max_offset = document_height - (size_t)terminal_rows;
        size_t offset = repl->viewport_offset;
        if (offset > max_offset) {
            offset = max_offset;
        }

        // When offset=0, show last terminal_rows of document
        // When offset=N, scroll up by N rows
        last_visible_row = document_height - 1 - offset;
        first_visible_row = last_visible_row + 1 - (size_t)terminal_rows;
    }

    // Determine which scrollback lines are visible
    if (first_visible_row > separator_row || scrollback_rows == 0) {
        // Viewport starts after all scrollback - no scrollback visible
        viewport_out->scrollback_start_line = 0;
        viewport_out->scrollback_lines_count = 0;
    } else {
        // Some scrollback is visible
        // Find logical line at first_visible_row
        size_t start_line = 0;
        size_t row_offset = 0;

        if (scrollback_rows > 0 && first_visible_row < scrollback_rows) {  /* LCOV_EXCL_BR_LINE */
            res_t result = ik_scrollback_find_logical_line_at_physical_row(
                repl->scrollback,
                first_visible_row,
                &start_line,
                &row_offset
                );
            if (is_err(&result)) { /* LCOV_EXCL_BR_LINE */
                PANIC("Failed to find logical line at physical row"); /* LCOV_EXCL_LINE */
            }
        }

        // Count how many scrollback lines are visible
        size_t lines_count = 0;
        size_t current_row = first_visible_row;
        for (size_t i = start_line; i < scrollback_line_count && current_row < separator_row; i++) {  /* LCOV_EXCL_BR_LINE */
            current_row += repl->scrollback->layouts[i].physical_lines;
            lines_count++;
            if (current_row > last_visible_row)break;
        }

        viewport_out->scrollback_start_line = start_line;
        viewport_out->scrollback_lines_count = lines_count;
    }

    // Calculate where input buffer appears in viewport
    // Input buffer always occupies at least 1 row in document (even when empty)
    if (input_buffer_start_doc_row <= last_visible_row) {
        // Input buffer is at least partially visible
        if (input_buffer_start_doc_row >= first_visible_row) {  /* LCOV_EXCL_BR_LINE */
            // Input buffer starts within viewport
            viewport_out->input_buffer_start_row = input_buffer_start_doc_row - first_visible_row;
        } else {  /* LCOV_EXCL_BR_LINE */
            // Input buffer starts before viewport (shouldn't happen)  /* LCOV_EXCL_LINE */
            viewport_out->input_buffer_start_row = 0;  /* LCOV_EXCL_LINE */
        }  /* LCOV_EXCL_LINE */
    } else {
        // Input buffer is completely off-screen
        viewport_out->input_buffer_start_row = (size_t)terminal_rows;
    }

    // Calculate separator visibility
    // Separator is at document row separator_row
    // It's visible if it's in the range [first_visible_row, last_visible_row]
    viewport_out->separator_visible = separator_row >= first_visible_row &&
                                      separator_row <= last_visible_row;

    return OK(repl);
}

res_t ik_repl_render_frame(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->render != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->input_buffer != NULL);   /* LCOV_EXCL_BR_LINE */

    // Calculate viewport to determine what to render
    ik_viewport_t viewport;
    res_t result = ik_repl_calculate_viewport(repl, &viewport);
    if (is_err(&result))return result;  /* LCOV_EXCL_LINE */

    // Get input buffer text
    char *text = NULL;
    size_t text_len = 0;
    ik_input_buffer_get_text(repl->input_buffer, &text, &text_len);

    // Get cursor byte offset
    size_t cursor_byte_offset = 0;
    size_t cursor_grapheme = 0;
    ik_input_buffer_get_cursor_position(repl->input_buffer, &cursor_byte_offset, &cursor_grapheme);

    // Determine visibility of separator and input buffer (unified document model)
    // Separator visibility is calculated in ik_repl_calculate_viewport()
    // Input buffer visible when input_buffer_start_row in [0, terminal_rows-1]
    bool separator_visible = viewport.separator_visible;
    bool input_buffer_visible = viewport.input_buffer_start_row < (size_t)repl->term->screen_rows;

    // Render combined view with conditional separator/input_buffer
    return ik_render_combined(repl->render,
                              repl->scrollback,
                              viewport.scrollback_start_line,
                              viewport.scrollback_lines_count,
                              text,
                              text_len,
                              cursor_byte_offset,
                              separator_visible,
                              input_buffer_visible);
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
        if (is_err(&result))return result;  // LCOV_EXCL_LINE - Defensive, allocation failures are rare
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
