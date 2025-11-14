// Direct terminal rendering (no VTerm)
#include "render.h"
#include "error.h"
#include "wrapper.h"
#include <assert.h>
#include <inttypes.h>
#include <talloc.h>
#include <utf8proc.h>

// Internal structure for cursor screen position
typedef struct {
    int32_t screen_row;
    int32_t screen_col;
} cursor_screen_pos_t;

// Forward declaration for internal testing function
res_t calculate_cursor_screen_position(void *ctx,
                                       const char *text,
                                       size_t text_len,
                                       size_t cursor_byte_offset,
                                       int32_t terminal_width,
                                       cursor_screen_pos_t *pos_out);

// Create render context
res_t ik_render_create(void *parent, int32_t rows, int32_t cols,
                       int32_t tty_fd, ik_render_ctx_t **ctx_out)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE
    assert(ctx_out != NULL); // LCOV_EXCL_BR_LINE

    // Validate dimensions
    if (rows <= 0 || cols <= 0) {
        return ERR(parent, INVALID_ARG, "Invalid terminal dimensions: rows=%" PRId32 ", cols=%" PRId32, rows, cols);
    }

    // Allocate context
    ik_render_ctx_t *ctx = ik_talloc_zero_wrapper(parent, sizeof(ik_render_ctx_t));
    if (!ctx) {
        return ERR(parent, OOM, "Failed to allocate render context");
    }

    // Initialize fields
    ctx->rows = rows;
    ctx->cols = cols;
    ctx->tty_fd = tty_fd;

    *ctx_out = ctx;
    return OK(ctx);
}

// Calculate cursor screen position (row, col) from byte offset
// Internal function for testing - calculates where cursor should appear on screen
// accounting for UTF-8 character widths and line wrapping
res_t calculate_cursor_screen_position(
    void *ctx,
    const char *text, size_t text_len,
    size_t cursor_byte_offset,
    int32_t terminal_width,
    cursor_screen_pos_t *pos_out
    )
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(text != NULL);     // LCOV_EXCL_BR_LINE
    assert(pos_out != NULL);  // LCOV_EXCL_BR_LINE

    int32_t row = 0;
    int32_t col = 0;
    size_t pos = 0;

    // Iterate through text up to cursor position
    while (pos < cursor_byte_offset) {
        // Handle newlines
        if (text[pos] == '\n') {
            row++;
            col = 0;
            pos++;
            continue;
        }

        // Decode UTF-8 codepoint
        utf8proc_int32_t cp;
        utf8proc_ssize_t bytes = utf8proc_iterate(
            (const utf8proc_uint8_t *)(text + pos),
            (utf8proc_ssize_t)(text_len - pos),
            &cp
            );

        if (bytes <= 0) {
            return ERR(ctx, INVALID_ARG, "Invalid UTF-8 at byte offset %zu", pos);
        }

        // Get display width (accounts for wide chars like CJK, combining chars)
        // Note: utf8proc_charwidth may return negative for control characters,
        // but in practice this doesn't occur for valid displayable text
        int32_t width = utf8proc_charwidth(cp);

        // Check for line wrap
        if (col + width > terminal_width) {
            row++;
            col = 0;
        }

        col += width;
        pos += (size_t)bytes;
    }

    // If cursor is exactly at terminal width, wrap to next line
    if (col == terminal_width) {
        row++;
        col = 0;
    }

    pos_out->screen_row = row;
    pos_out->screen_col = col;
    return OK(ctx);
}

// Render workspace to terminal (text + cursor positioning)
res_t ik_render_workspace(ik_render_ctx_t *ctx,
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
    size_t newline_count = 0;
    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            newline_count++;
        }
    }

    // Allocate framebuffer (~64KB should be enough for typical terminal content)
    // Clear screen (4 bytes) + home escape (3 bytes) + text + newlines + cursor position escape (~15 bytes) + safety margin
    // Note: Theoretical integer overflow if text_len + newline_count > SIZE_MAX - 27.
    // This would require ~18 exabytes of text on 64-bit systems. In practice, the
    // system OOMs and talloc allocation fails long before overflow can occur. This
    // edge case is not tested as it cannot be reproduced without exabytes of RAM.
    size_t buffer_size = 7 + text_len + newline_count + 20;
    char *framebuffer = ik_talloc_array_wrapper(ctx, sizeof(char), buffer_size);
    if (!framebuffer) {
        return ERR(ctx, OOM, "Failed to allocate framebuffer");
    }

    // Build framebuffer
    size_t offset = 0;

    // Add clear screen escape: \x1b[2J
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = '2';
    framebuffer[offset++] = 'J';

    // Add home cursor escape: \x1b[H
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = 'H';

    // Copy text, converting \n to \r\n for proper terminal display
    if (text_len > 0) {
        for (size_t i = 0; i < text_len; i++) {
            if (text[i] == '\n') {
                framebuffer[offset++] = '\r';
                framebuffer[offset++] = '\n';
            } else {
                framebuffer[offset++] = text[i];
            }
        }
    }

    // Add cursor positioning escape: \x1b[<row+1>;<col+1>H
    // Terminal coordinates are 1-based, our internal are 0-based
    char *cursor_escape = ik_talloc_asprintf_wrapper(ctx, "\x1b[%" PRId32 ";%" PRId32 "H",
                                                     cursor_pos.screen_row + 1,
                                                     cursor_pos.screen_col + 1);
    if (!cursor_escape) {
        talloc_free(framebuffer);
        return ERR(ctx, OOM, "Failed to allocate cursor escape string");
    }

    // Append cursor escape to framebuffer
    for (size_t i = 0; cursor_escape[i] != '\0'; i++) {
        framebuffer[offset++] = cursor_escape[i];
    }
    talloc_free(cursor_escape);

    // Single write to terminal
    ssize_t bytes_written = ik_write_wrapper(ctx->tty_fd, framebuffer, offset);
    talloc_free(framebuffer);

    if (bytes_written < 0) {
        return ERR(ctx, IO, "Failed to write to terminal");
    }

    return OK(ctx);
}

// Render scrollback lines to terminal (Phase 4)
res_t ik_render_scrollback(ik_render_ctx_t *ctx,
                           ik_scrollback_t *scrollback,
                           size_t start_line,
                           size_t line_count,
                           int32_t *rows_used_out)
{
    assert(ctx != NULL);                    // LCOV_EXCL_BR_LINE
    assert(scrollback != NULL);             // LCOV_EXCL_BR_LINE
    assert(rows_used_out != NULL);          // LCOV_EXCL_BR_LINE

    // Handle empty scrollback or no lines to render
    if (line_count == 0) {
        *rows_used_out = 0;
        return OK(ctx);
    }

    // Ensure scrollback layout is up to date
    res_t result = ik_scrollback_ensure_layout(scrollback, ctx->cols);
    if (is_err(&result)) { /* LCOV_EXCL_LINE */
        return result;      /* LCOV_EXCL_LINE */
    }

    // Validate range
    size_t total_lines = ik_scrollback_get_line_count(scrollback);
    if (start_line >= total_lines) {
        return ERR(ctx, INVALID_ARG, "start_line (%zu) >= total_lines (%zu)", start_line, total_lines);
    }

    // Clamp line_count to available lines
    size_t end_line = start_line + line_count;
    if (end_line > total_lines) {
        end_line = total_lines;
        line_count = end_line - start_line;
    }

    // Calculate total buffer size needed
    // Clear screen (4 bytes) + home (3 bytes) + For each line: text + \r\n (for newline conversion)
    size_t total_size = 7;  // Clear screen + home cursor escapes
    for (size_t i = start_line; i < end_line; i++) {
        const char *line_text = NULL;
        size_t line_len = 0;
        result = ik_scrollback_get_line_text(scrollback, i, &line_text, &line_len);
        if (is_err(&result)) { /* LCOV_EXCL_LINE */
            return result;      /* LCOV_EXCL_LINE */
        }

        // Count newlines in this line
        size_t newline_count = 0;
        for (size_t j = 0; j < line_len; j++) {
            if (line_text[j] == '\n') {
                newline_count++;
            }
        }

        total_size += line_len + newline_count + 2;  // +2 for final \r\n
    }

    // Allocate framebuffer
    char *framebuffer = ik_talloc_array_wrapper(ctx, sizeof(char), total_size);
    if (!framebuffer) { /* LCOV_EXCL_BR_LINE */
        return ERR(ctx, OOM, "Failed to allocate scrollback framebuffer"); /* LCOV_EXCL_LINE */
    }

    // Build framebuffer
    size_t offset = 0;
    int32_t rows_used = 0;

    // Add clear screen escape: \x1b[2J
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = '2';
    framebuffer[offset++] = 'J';

    // Add home cursor escape: \x1b[H
    framebuffer[offset++] = '\x1b';
    framebuffer[offset++] = '[';
    framebuffer[offset++] = 'H';

    for (size_t i = start_line; i < end_line; i++) {
        const char *line_text = NULL;
        size_t line_len = 0;
        result = ik_scrollback_get_line_text(scrollback, i, &line_text, &line_len);
        if (is_err(&result)) { /* LCOV_EXCL_LINE */
            talloc_free(framebuffer); /* LCOV_EXCL_LINE */
            return result;      /* LCOV_EXCL_LINE */
        }

        // Copy line text, converting \n to \r\n
        for (size_t j = 0; j < line_len; j++) {
            if (line_text[j] == '\n') {
                framebuffer[offset++] = '\r';
                framebuffer[offset++] = '\n';
            } else {
                framebuffer[offset++] = line_text[j];
            }
        }

        // Add \r\n at end of each line
        framebuffer[offset++] = '\r';
        framebuffer[offset++] = '\n';

        // Count rows used by this line (accounts for wrapping)
        rows_used += (int32_t)scrollback->layouts[i].physical_lines;
    }

    // Write framebuffer to terminal (offset always > 0 when line_count > 0)
    ssize_t bytes_written = ik_write_wrapper(ctx->tty_fd, framebuffer, offset);
    talloc_free(framebuffer);

    if (bytes_written < 0) { /* LCOV_EXCL_LINE */
        return ERR(ctx, IO, "Failed to write scrollback to terminal"); /* LCOV_EXCL_LINE */
    }

    *rows_used_out = rows_used;
    return OK(ctx);
}
