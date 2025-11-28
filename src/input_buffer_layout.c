/**
 * @file input_buffer_layout.c
 * @brief Input buffer layout caching implementation
 */

#include "input_buffer.h"
#include "error.h"
#include <assert.h>
#include <inttypes.h>
#include <utf8proc.h>

/**
 * @brief Calculate display width of UTF-8 text
 *
 * @param text UTF-8 text
 * @param len Length in bytes
 * @return Display width in columns
 */
static size_t calculate_display_width(const char *text, size_t len)
{
    if (text == NULL || len == 0)return 0;  // LCOV_EXCL_LINE - defensive: text is never NULL when len > 0

    size_t display_width = 0;
    size_t pos = 0;

    while (pos < len) {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t bytes_read = utf8proc_iterate((const utf8proc_uint8_t *)(text + pos),
                                                       (utf8proc_ssize_t)(len - pos),
                                                       &codepoint);

        /* Invalid UTF-8 - treat as 1 column and continue (defensive: input buffer only contains valid UTF-8) */
        if (bytes_read <= 0) { /* LCOV_EXCL_BR_LINE */
            display_width++; pos++; continue; /* LCOV_EXCL_LINE */
        }

        /* Get character width */
        int32_t char_width = utf8proc_charwidth(codepoint);
        if (char_width >= 0) { /* LCOV_EXCL_BR_LINE - defensive: utf8proc_charwidth returns >= 0 for all valid codepoints */
            display_width += (size_t)char_width;
        }

        pos += (size_t)bytes_read;
    }

    return display_width;
}

void ik_input_buffer_ensure_layout(ik_input_buffer_t *input_buffer, int32_t terminal_width)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    /* If layout is clean and width unchanged, no recalculation needed */
    if (input_buffer->layout_dirty == 0 && input_buffer->cached_width == terminal_width) {
        return;
    }

    /* Get input buffer text */
    char *text;
    size_t text_len;
    ik_input_buffer_get_text(input_buffer, &text, &text_len); // Never fails

    /* Empty input buffer: 0 physical lines (Bug #10 fix) */
    if (text == NULL || text_len == 0) { /* LCOV_EXCL_BR_LINE - defensive: text is NULL only when text_len is 0 */
        input_buffer->physical_lines = 0;
        input_buffer->cached_width = terminal_width;
        input_buffer->layout_dirty = 0;
        return;
    }

    /* Calculate physical lines by scanning text */
    size_t physical_lines = 0;
    size_t line_start = 0;

    for (size_t i = 0; i <= text_len; i++) {
        /* Found newline or end of text */
        if (i == text_len || text[i] == '\n') {
            size_t line_len = i - line_start;

            if (line_len == 0) {
                /* Empty line (just newline) */
                physical_lines += 1;
            } else {
                /* Calculate display width of this logical line */
                size_t display_width = calculate_display_width(text + line_start, line_len);

                /* Calculate how many physical lines it occupies */
                if (display_width == 0) { /* LCOV_EXCL_BR_LINE - rare: only zero-width characters */
                    physical_lines += 1;
                } else if (terminal_width > 0) { /* LCOV_EXCL_BR_LINE - defensive: terminal_width is always > 0 */
                    /* Integer division with rounding up */
                    physical_lines += (display_width + (size_t)terminal_width - 1) / (size_t)terminal_width;
                } else {
                    /* Defensive: terminal_width should never be <= 0 */
                    physical_lines += 1; /* LCOV_EXCL_LINE */
                }
            }

            /* Move to next line */
            line_start = i + 1;
        }
    }

    /* If text doesn't end with newline, we already counted the last line */
    /* If text ends with newline, we counted it above */

    /* Update cached values */
    input_buffer->physical_lines = physical_lines;
    input_buffer->cached_width = terminal_width;
    input_buffer->layout_dirty = 0;
}

void ik_input_buffer_invalidate_layout(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */
    input_buffer->layout_dirty = 1;
}

size_t ik_input_buffer_get_physical_lines(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */
    return input_buffer->physical_lines;
}
