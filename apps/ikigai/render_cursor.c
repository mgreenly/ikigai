/**
 * @file render_cursor.c
 * @brief Cursor screen position calculation for rendering
 */

#include "apps/ikigai/render_cursor.h"
#include "apps/ikigai/ansi.h"
#include "shared/error.h"
#include <assert.h>
#include <utf8proc.h>


#include "shared/poison.h"
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

        // Skip ANSI escape sequences
        size_t skip = ik_ansi_skip_csi(text, text_len, pos);
        if (skip > 0) {
            pos += skip;
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
