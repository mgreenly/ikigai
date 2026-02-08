// Direct ANSI terminal rendering
#include "apps/ikigai/render.h"

#include "shared/error.h"
#include "shared/panic.h"
#include "apps/ikigai/render_cursor.h"
#include "apps/ikigai/render_text.h"
#include "shared/wrapper.h"
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <talloc.h>
#include <utf8proc.h>


#include "shared/poison.h"
// Create render context
res_t ik_render_create(TALLOC_CTX *ctx, int32_t rows, int32_t cols,
                       int32_t tty_fd, ik_render_ctx_t **render_ctx_out)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(render_ctx_out != NULL); // LCOV_EXCL_BR_LINE

    // Validate dimensions
    if (rows <= 0 || cols <= 0) {
        return ERR(ctx, INVALID_ARG, "Invalid terminal dimensions: rows=%" PRId32 ", cols=%" PRId32, rows, cols);
    }

    // Allocate context
    ik_render_ctx_t *render_ctx = talloc_zero_(ctx, sizeof(ik_render_ctx_t));
    if (render_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Initialize fields
    render_ctx->rows = rows;
    render_ctx->cols = cols;
    render_ctx->tty_fd = tty_fd;

    *render_ctx_out = render_ctx;
    return OK(render_ctx);
}

// Render input buffer to terminal (text + cursor positioning)
res_t ik_render_input_buffer(ik_render_ctx_t *ctx,
                             const char *text, size_t text_len,
                             size_t cursor_byte_offset)
{
    assert(ctx != NULL);                          // LCOV_EXCL_BR_LINE
    assert(text != NULL || text_len == 0);        // LCOV_EXCL_BR_LINE

    // Calculate cursor screen position
    cursor_screen_pos_t cursor_pos = {0};
    res_t result;

    // Handle empty text specially (cursor at home position)
    if (text == NULL || text_len == 0) {
        cursor_pos.screen_row = 0;
        cursor_pos.screen_col = 0;
    } else {
        result = calculate_cursor_screen_position(ctx, text, text_len, cursor_byte_offset, ctx->cols, &cursor_pos);
        if (is_err(&result)) {
            return result;
        }
    }

    // Count newlines to calculate buffer size (each \n becomes \r\n, adding 1 byte per newline)
    size_t newline_count = ik_render_count_newlines(text, text_len);

    // Allocate framebuffer (~64KB should be enough for typical terminal content)
    // Clear screen (4 bytes) + home escape (3 bytes) + text + newlines + cursor position escape (~15 bytes) + safety margin
    // Note: Theoretical integer overflow if text_len + newline_count > SIZE_MAX - 27.
    // This would require ~18 exabytes of text on 64-bit systems. In practice, the
    // system OOMs and talloc allocation fails long before overflow can occur. This
    // edge case is not tested as it cannot be reproduced without exabytes of RAM.
    size_t buffer_size = 7 + text_len + newline_count + 20;
    char *framebuffer = talloc_array_(ctx, sizeof(char), buffer_size);
    if (framebuffer == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Build framebuffer
    size_t offset = 0;

    // Add clear screen escape: \x1b[2J
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = '2';
    framebuffer[offset++] = 'J';

    // Hide cursor FIRST to prevent flicker during rendering: \x1b[?25l
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = '?';
    framebuffer[offset++] = '2';
    framebuffer[offset++] = '5';
    framebuffer[offset++] = 'l';

    // Add home cursor escape: \x1b[H
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = 'H';

    // Copy text, converting \n to \r\n for proper terminal display
    if (text_len > 0) {
        offset += ik_render_copy_text_with_crlf(&framebuffer[offset], text, text_len);
    }

    // Add cursor positioning escape: \x1b[<row+1>;<col+1>H
    // Terminal coordinates are 1-based, our internal are 0-based
    char *cursor_escape = talloc_asprintf_(ctx, "\x1b[%" PRId32 ";%" PRId32 "H",
                                           cursor_pos.screen_row + 1,
                                           cursor_pos.screen_col + 1);
    if (cursor_escape == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Append cursor escape to framebuffer
    for (size_t i = 0; cursor_escape[i] != '\0'; i++) {
        framebuffer[offset++] = cursor_escape[i];
    }
    talloc_free(cursor_escape);

    // Single write to terminal
    ssize_t bytes_written = posix_write_(ctx->tty_fd, framebuffer, offset);
    talloc_free(framebuffer);

    if (bytes_written < 0) {
        return ERR(ctx, IO, "Failed to write to terminal");
    }

    return OK(ctx);
}

// Render combined scrollback + input buffer in single atomic write (Phase 4 Task 4.4)
// render_separator and render_input_buffer control visibility (unified document model)
res_t ik_render_combined(ik_render_ctx_t *ctx,
                         ik_scrollback_t *scrollback,
                         size_t scrollback_start_line,
                         size_t scrollback_line_count,
                         const char *input_text,
                         size_t input_text_len,
                         size_t input_cursor_offset,
                         bool render_separator,
                         bool render_input_buffer)
{
    assert(ctx != NULL);                    // LCOV_EXCL_BR_LINE
    assert(scrollback != NULL);             // LCOV_EXCL_BR_LINE
    assert(input_text != NULL || input_text_len == 0);  // LCOV_EXCL_BR_LINE

    // Ensure scrollback layout is up to date
    ik_scrollback_ensure_layout(scrollback, ctx->cols);

    // Validate scrollback range
    size_t total_lines = ik_scrollback_get_line_count(scrollback);
    if (scrollback_line_count > 0 && scrollback_start_line >= total_lines) {
        return ERR(ctx, INVALID_ARG, "scrollback_start_line (%zu) >= total_lines (%zu)",
                   scrollback_start_line, total_lines);
    }

    // Clamp scrollback line_count to available lines
    size_t scrollback_end_line = scrollback_start_line + scrollback_line_count;
    if (scrollback_end_line > total_lines) {
        scrollback_end_line = total_lines;
        scrollback_line_count = scrollback_end_line - scrollback_start_line;
    }

    // Calculate total physical rows used by scrollback
    int32_t scrollback_rows_used = 0;
    for (size_t i = scrollback_start_line; i < scrollback_end_line; i++) {
        scrollback_rows_used += (int32_t)scrollback->layouts[i].physical_lines;
    }

    // Calculate cursor screen position for input buffer
    cursor_screen_pos_t input_cursor_pos = {0};
    if (input_text_len > 0) {
        res_t result = calculate_cursor_screen_position(ctx, input_text, input_text_len,
                                                        input_cursor_offset, ctx->cols,
                                                        &input_cursor_pos);
        if (is_err(&result)) {
            return result;
        }
    }

    // Offset input buffer cursor by scrollback rows
    int32_t final_cursor_row = scrollback_rows_used + input_cursor_pos.screen_row;
    int32_t final_cursor_col = input_cursor_pos.screen_col;

    // Calculate buffer size needed
    // Clear (4) + Home (3) + scrollback content + separator (cols+2) + input buffer content + cursor visibility (6) + cursor position (~20)
    size_t buffer_size = 7 + 6 + 20;  // Base escapes + cursor visibility

    // Add separator line if visible
    if (render_separator) {
        buffer_size += (size_t)ctx->cols + 2;  // separator line + \r\n
    }

    // Add scrollback size
    for (size_t i = scrollback_start_line; i < scrollback_end_line; i++) {
        const char *line_text = NULL;
        size_t line_len = 0;
        res_t result = ik_scrollback_get_line_text(scrollback, i, &line_text, &line_len);
        if (is_err(&result)) return result;  // LCOV_EXCL_LINE

        // Count newlines
        size_t newline_count = ik_render_count_newlines(line_text, line_len);

        buffer_size += line_len + newline_count + 2;  // +2 for final \r\n
    }

    // Add input buffer size if visible
    if (render_input_buffer && input_text_len > 0) {
        size_t ib_newline_count = ik_render_count_newlines(input_text, input_text_len);
        buffer_size += input_text_len + ib_newline_count;
    }

    // Allocate framebuffer
    char *framebuffer = talloc_array_(ctx, sizeof(char), buffer_size);
    if (framebuffer == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t offset = 0;

    // Clear screen: \x1b[2J
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = '2';
    framebuffer[offset++] = 'J';

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

    // Write scrollback lines
    for (size_t i = scrollback_start_line; i < scrollback_end_line; i++) {
        const char *line_text = NULL;
        size_t line_len = 0;
        res_t result = ik_scrollback_get_line_text(scrollback, i, &line_text, &line_len);
        if (is_err(&result)) return result;  // LCOV_EXCL_LINE - defensive: loop bounds validated

        // Copy line text, converting \n to \r\n
        offset += ik_render_copy_text_with_crlf(&framebuffer[offset], line_text, line_len);

        // Add \r\n at end of each scrollback line
        // UNLESS it's the last line AND separator/input_buffer are both off-screen
        // (Bug #10 fix - prevents terminal scroll when last line is on last terminal row)
        bool is_last_scrollback_line = (i == scrollback_end_line - 1);
        bool nothing_after = !render_separator && !render_input_buffer;
        if (!is_last_scrollback_line || !nothing_after) {
            framebuffer[offset++] = '\r';
            framebuffer[offset++] = '\n';
        }
    }

    // Add separator line between scrollback and input buffer (if visible)
    if (render_separator) {
        for (int32_t i = 0; i < ctx->cols; i++) {
            framebuffer[offset++] = '-';
        }

        // Only add \r\n after separator if input buffer is being rendered
        // Otherwise, the terminal scrolls up when separator is on last line
        if (render_input_buffer) {
            framebuffer[offset++] = '\r';
            framebuffer[offset++] = '\n';
        }

        // Account for separator in cursor position
        final_cursor_row++;
    }

    // Write input buffer text (if visible)
    if (render_input_buffer && input_text_len > 0) {
        offset += ik_render_copy_text_with_crlf(&framebuffer[offset], input_text, input_text_len);
    }

    // Set final cursor visibility state
    // Show cursor if input buffer visible, hide if not
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = '?';
    framebuffer[offset++] = '2';
    framebuffer[offset++] = '5';
    framebuffer[offset++] = render_input_buffer ? 'h' : 'l';

    // Position cursor when input buffer visible
    if (render_input_buffer) {
        char *cursor_escape = talloc_asprintf_(ctx, "\x1b[%" PRId32 ";%" PRId32 "H",
                                               final_cursor_row + 1, final_cursor_col + 1);
        if (cursor_escape == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        for (size_t i = 0; cursor_escape[i] != '\0'; i++) {
            framebuffer[offset++] = cursor_escape[i];
        }
        talloc_free(cursor_escape);
    }

    // Single atomic write
    ssize_t bytes_written = posix_write_(ctx->tty_fd, framebuffer, offset);
    talloc_free(framebuffer);

    if (bytes_written < 0) {
        return ERR(ctx, IO, "Failed to write combined frame to terminal");
    }

    return OK(ctx);
}
