#include "repl.h"

#include "agent.h"
#include "panic.h"
#include "render_cursor.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <talloc.h>

// Calculate total document height.
// Layer order: banner, scrollback, spinner, separator, input, completion, status.
size_t ik_repl_calculate_document_height(const ik_repl_ctx_t *repl)
{
    size_t banner_rows = repl->current->banner_visible ? 6 : 0;
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(repl->current->scrollback);
    size_t spinner_rows = repl->current->spinner_state.visible ? 1 : 0;
    size_t separator_rows = 1;  // Always visible
    size_t input_buffer_rows = ik_input_buffer_get_physical_lines(repl->current->input_buffer);
    size_t input_rows = repl->current->input_buffer_visible
                        ? ((input_buffer_rows == 0) ? 1 : input_buffer_rows)
                        : 0;
    size_t completion_rows = (repl->current->completion != NULL) ? repl->current->completion->count : 0;
    size_t status_rows = repl->current->status_visible ? 2 : 0;

    return banner_rows + scrollback_rows + spinner_rows + separator_rows + input_rows + completion_rows + status_rows;
}

res_t ik_repl_calculate_viewport(ik_repl_ctx_t *repl, ik_viewport_t *viewport_out)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(viewport_out != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->shared->term != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->current->input_buffer != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->current->scrollback != NULL);   /* LCOV_EXCL_BR_LINE */

    // Ensure input buffer layout is up to date
    ik_input_buffer_ensure_layout(repl->current->input_buffer, repl->shared->term->screen_cols);

    // Ensure scrollback layout is up to date
    ik_scrollback_ensure_layout(repl->current->scrollback, repl->shared->term->screen_cols);

    // Get component sizes (must match layer cake order)
    size_t banner_rows = repl->current->banner_visible ? 6 : 0;
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(repl->current->scrollback);
    size_t scrollback_line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    size_t spinner_rows = repl->current->spinner_state.visible ? 1 : 0;
    int32_t terminal_rows = repl->shared->term->screen_rows;

    // Calculate document dimensions (must match layer cake order: banner, scrollback, spinner, separator, input, ...)
    size_t separator_row = banner_rows + scrollback_rows + spinner_rows;  // Separator is at this document row (0-indexed)
    size_t input_buffer_start_doc_row = separator_row + 1;  // Input buffer starts after separator
    size_t document_height = ik_repl_calculate_document_height(repl);

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
        size_t offset = repl->current->viewport_offset;
        if (offset > max_offset) {
            offset = max_offset;
        }

        // When offset=0, show last terminal_rows of document
        // When offset=N, scroll up by N rows
        last_visible_row = document_height - 1 - offset;
        first_visible_row = last_visible_row + 1 - (size_t)terminal_rows;
    }

    // Determine which scrollback lines are visible
    // Scrollback occupies document rows [banner_rows, separator_row - 1]
    if (first_visible_row >= separator_row || scrollback_rows == 0) {
        // Viewport starts at or after separator - no scrollback visible
        viewport_out->scrollback_start_line = 0;
        viewport_out->scrollback_lines_count = 0;
    } else {
        // Some scrollback is visible
        // Find logical line at first visible scrollback row
        size_t start_line = 0;
        size_t row_offset = 0;

        // Convert document row to scrollback-relative row
        size_t scrollback_first_row = (first_visible_row > banner_rows)
                                        ? first_visible_row - banner_rows
                                        : 0;

        if (scrollback_rows > 0 && scrollback_first_row < scrollback_rows) {  /* LCOV_EXCL_BR_LINE */
            res_t result = ik_scrollback_find_logical_line_at_physical_row(
                repl->current->scrollback,
                scrollback_first_row,
                &start_line,
                &row_offset
                );
            if (is_err(&result)) { /* LCOV_EXCL_BR_LINE */
                PANIC("Failed to find logical line at physical row"); /* LCOV_EXCL_LINE */
            }
        }

        // Count how many scrollback lines are visible
        size_t lines_count = 0;
        size_t current_doc_row = banner_rows + scrollback_first_row;
        for (size_t i = start_line; i < scrollback_line_count && current_doc_row < separator_row; i++) {  /* LCOV_EXCL_BR_LINE */
            current_doc_row += repl->current->scrollback->layouts[i].physical_lines;
            lines_count++;
            if (current_doc_row > last_visible_row) break;
        }

        viewport_out->scrollback_start_line = start_line;
        viewport_out->scrollback_lines_count = lines_count;
    }

    // Calculate where input buffer appears in viewport
    // Input buffer always occupies at least 1 row in document (even when empty)
    if (input_buffer_start_doc_row <= last_visible_row) {
        // Input buffer is at least partially visible
        if (input_buffer_start_doc_row >= first_visible_row) {
            // Input buffer starts within viewport
            viewport_out->input_buffer_start_row = input_buffer_start_doc_row - first_visible_row;
        } else {
            // Input buffer starts before viewport
            // This can occur when scrolling with a large input buffer
            viewport_out->input_buffer_start_row = 0;
        }
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
    assert(repl->shared->render != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->current->input_buffer != NULL);   /* LCOV_EXCL_BR_LINE */

    // Calculate viewport to determine what to render
    ik_viewport_t viewport;
    res_t result = ik_repl_calculate_viewport(repl, &viewport);
    if (is_err(&result)) return result; /* LCOV_EXCL_LINE */

    // Get input buffer text
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);

    // Get cursor byte offset
    size_t cursor_byte_offset = 0;
    size_t cursor_grapheme = 0;
    ik_input_buffer_get_cursor_position(repl->current->input_buffer, &cursor_byte_offset, &cursor_grapheme);

    // Determine visibility of separator and input buffer (unified document model)
    bool separator_visible = viewport.separator_visible;
    bool input_buffer_visible = viewport.input_buffer_start_row < (size_t)repl->shared->term->screen_rows;

    // Fall back to old rendering path if layer cake not initialized (for tests)
    if (repl->current->layer_cake == NULL) {
        return ik_render_combined(repl->shared->render,
                                  repl->current->scrollback,
                                  viewport.scrollback_start_line,
                                  viewport.scrollback_lines_count,
                                  text,
                                  text_len,
                                  cursor_byte_offset,
                                  separator_visible,
                                  input_buffer_visible);
    }

    // Update layer reference fields (respecting REPL state)
    repl->current->separator_visible = separator_visible;

    // State-based visibility (Phase 1.6 Task 6.4)
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    ik_agent_state_t current_state = repl->current->state;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    if (current_state == IK_AGENT_STATE_WAITING_FOR_LLM) {
        // When waiting for LLM: hide input, show spinner
        repl->current->spinner_state.visible = true;
        repl->current->input_buffer_visible = false;
        input_buffer_visible = false;  // Update local variable for cursor control
    } else {
        // When idle: show input (if in viewport), hide spinner
        repl->current->spinner_state.visible = false;
        repl->current->input_buffer_visible = input_buffer_visible;
    }

    repl->current->input_text = (text != NULL) ? text : "";
    repl->current->input_text_len = text_len;

    // Calculate document dimensions for layer cake viewport
    size_t document_height = ik_repl_calculate_document_height(repl);
    int32_t terminal_rows = repl->shared->term->screen_rows;

    size_t first_visible_row;
    if (document_height <= (size_t)terminal_rows) {
        first_visible_row = 0;
    } else {
        size_t max_offset = document_height - (size_t)terminal_rows;
        size_t offset = repl->current->viewport_offset;
        if (offset > max_offset) {
            offset = max_offset;
        }
        size_t last_visible_row = document_height - 1 - offset;
        first_visible_row = last_visible_row + 1 - (size_t)terminal_rows;
    }

    // Configure layer cake viewport
    repl->current->layer_cake->viewport_row = first_visible_row;
    repl->current->layer_cake->viewport_height = (size_t)terminal_rows;

    // Update debug info for separator display
    repl->debug_viewport_offset = repl->current->viewport_offset;
    repl->debug_viewport_row = first_visible_row;
    repl->debug_viewport_height = (size_t)terminal_rows;
    repl->debug_document_height = document_height;

    // Render layers to output buffer
    ik_output_buffer_t *output = ik_output_buffer_create(repl, 4096);

    ik_layer_cake_render(repl->current->layer_cake, output, (size_t)repl->shared->term->screen_cols);

    // Bug fix: When rendered content fills the terminal, the trailing \r\n
    // causes the terminal to scroll up by 1 row. This makes cursor positioning
    // land on the wrong row. Remove trailing \r\n when content fills screen.
    if (document_height >= (size_t)terminal_rows && output->size >= 2) {  // LCOV_EXCL_BR_LINE - Defensive: output->size typically >2
        if (output->data[output->size - 2] == '\r' && output->data[output->size - 1] == '\n') {  // LCOV_EXCL_BR_LINE - Defensive: ends with \r\n
            output->size -= 2;
        }
    }

    // Calculate cursor position (offset by scrollback rows if input buffer is visible)
    cursor_screen_pos_t input_cursor_pos = {0};
    int32_t final_cursor_row = 0;
    int32_t final_cursor_col = 0;

    if (input_buffer_visible && text_len > 0) {
        // Input buffer always contains valid UTF-8 (validated at insertion)
        result = calculate_cursor_screen_position(repl, text, text_len,
                                                  cursor_byte_offset, repl->shared->term->screen_cols,
                                                  &input_cursor_pos);
        assert(is_ok(&result));  // LCOV_EXCL_BR_LINE
        // Offset cursor by viewport position of input buffer
        // input_buffer_start_row already accounts for separator (it's input_buffer_start_doc_row - first_visible_row)
        final_cursor_row = (int32_t)viewport.input_buffer_start_row + input_cursor_pos.screen_row;
        final_cursor_col = input_cursor_pos.screen_col;
    } else if (input_buffer_visible) {
        // Empty input buffer - cursor at start of input area
        final_cursor_row = (int32_t)viewport.input_buffer_start_row;
        final_cursor_col = 0;
    }

    // Build framebuffer with terminal control sequences
    size_t framebuffer_size = 6 + 3 + output->size + 3 + 6 + 20;  // hide cursor + home + content + clear-to-end + cursor visibility + position
    char *framebuffer = talloc_array_(repl, sizeof(char), framebuffer_size);
    if (framebuffer == NULL) PANIC("Out of memory"); /* LCOV_EXCL_BR_LINE */

    size_t offset = 0;

    // Hide cursor FIRST to prevent flicker during rendering: \x1b[?25l
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = '?';
    framebuffer[offset++] = '2';
    framebuffer[offset++] = '5';
    framebuffer[offset++] = 'l';

    // Home cursor: \x1b[H
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = 'H';

    // Copy rendered content from output buffer
    memcpy(framebuffer + offset, output->data, output->size);
    offset += output->size;
    talloc_free(output);

    // Clear from cursor to end of screen to remove old content: \x1b[J
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = 'J';

    // Set cursor visibility: show if input buffer visible, hide if not
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = '?';
    framebuffer[offset++] = '2';
    framebuffer[offset++] = '5';
    framebuffer[offset++] = input_buffer_visible ? 'h' : 'l';

    // Position cursor when input buffer visible
    if (input_buffer_visible) {
        char *cursor_escape = talloc_asprintf_(repl, "\x1b[%" PRId32 ";%" PRId32 "H",
                                               final_cursor_row + 1, final_cursor_col + 1);
        if (cursor_escape == NULL) PANIC("Out of memory"); /* LCOV_EXCL_BR_LINE */
        for (size_t i = 0; cursor_escape[i] != '\0'; i++) {
            framebuffer[offset++] = cursor_escape[i];
        }
        talloc_free(cursor_escape);
    }

#ifdef IKIGAI_DEV
    // Save framebuffer for dev dump (before freeing)
    talloc_free(repl->dev_framebuffer);
    repl->dev_framebuffer = talloc_memdup(repl, framebuffer, offset);
    repl->dev_framebuffer_len = offset;
    repl->dev_cursor_row = final_cursor_row;
    repl->dev_cursor_col = final_cursor_col;
#endif

    // Single atomic write
    ssize_t bytes_written = posix_write_(repl->shared->term->tty_fd, framebuffer, offset);
    talloc_free(framebuffer);

    if (bytes_written < 0) {
        return ERR(repl, IO, "Failed to write frame to terminal");
    }

    // Compute elapsed render time (for next frame's debug display)
    // LCOV_EXCL_START - Debug timing code requires precise control of render timing
    if (repl->render_start_us != 0) {
        struct timespec ts_end;
        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        uint64_t end_us = (uint64_t)ts_end.tv_sec * 1000000 + (uint64_t)ts_end.tv_nsec / 1000;
        repl->render_elapsed_us = end_us - repl->render_start_us;
        repl->render_start_us = 0;  // Reset so we don't re-compute on non-input renders
    }
    // LCOV_EXCL_STOP

    return OK(repl);
}
