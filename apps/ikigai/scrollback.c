/**
 * @file scrollback.c
 * @brief Scrollback buffer implementation
 */

#include "apps/ikigai/scrollback.h"

#include "apps/ikigai/ansi.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "apps/ikigai/scrollback_layout.h"
#include "apps/ikigai/scrollback_utils.h"
#include "shared/wrapper.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>
#include <utf8proc.h>


#include "shared/poison.h"
ik_scrollback_t *ik_scrollback_create(TALLOC_CTX *ctx, int32_t terminal_width)
{
    assert(terminal_width > 0);  // LCOV_EXCL_BR_LINE

    // Allocate scrollback struct
    ik_scrollback_t *scrollback = talloc_zero_(ctx, sizeof(ik_scrollback_t));
    if (scrollback == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Initialize metadata
    scrollback->count = 0;
    scrollback->capacity = 16;  // Initial capacity
    scrollback->cached_width = terminal_width;
    scrollback->total_physical_lines = 0;
    scrollback->buffer_used = 0;
    scrollback->buffer_capacity = 1024;  // Initial buffer size (1KB)

    // Allocate text_offsets array
    scrollback->text_offsets = talloc_array_(scrollback, sizeof(size_t), scrollback->capacity);
    if (scrollback->text_offsets == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Allocate text_lengths array
    scrollback->text_lengths = talloc_array_(scrollback, sizeof(size_t), scrollback->capacity);
    if (scrollback->text_lengths == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Allocate layouts array
    scrollback->layouts = talloc_array_(scrollback, sizeof(ik_line_layout_t), scrollback->capacity);
    if (scrollback->layouts == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Allocate text_buffer
    scrollback->text_buffer = talloc_array_(scrollback, sizeof(char), scrollback->buffer_capacity);
    if (scrollback->text_buffer == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return scrollback;
}

res_t ik_scrollback_append_line(ik_scrollback_t *scrollback,
                                const char *text, size_t length)
{
    assert(scrollback != NULL);              // LCOV_EXCL_BR_LINE
    assert(text != NULL || length == 0);     // LCOV_EXCL_BR_LINE

    // Check if we need to grow the line arrays
    if (scrollback->count >= scrollback->capacity) {
        size_t new_capacity = scrollback->capacity * 2;

        // Grow text_offsets
        size_t *new_offsets = talloc_realloc_(
            scrollback, scrollback->text_offsets, sizeof(size_t) * new_capacity);
        if (new_offsets == NULL) {     // LCOV_EXCL_BR_LINE
            return ERR(scrollback, OUT_OF_MEMORY, "Failed to reallocate text_offsets");
        }
        scrollback->text_offsets = new_offsets;

        // Grow text_lengths
        size_t *new_lengths = talloc_realloc_(
            scrollback, scrollback->text_lengths, sizeof(size_t) * new_capacity);
        if (new_lengths == NULL) {     // LCOV_EXCL_BR_LINE
            return ERR(scrollback, OUT_OF_MEMORY, "Failed to reallocate text_lengths");
        }
        scrollback->text_lengths = new_lengths;

        // Grow layouts
        ik_line_layout_t *new_layouts = talloc_realloc_(
            scrollback, scrollback->layouts, sizeof(ik_line_layout_t) * new_capacity);
        if (new_layouts == NULL) {     // LCOV_EXCL_BR_LINE
            return ERR(scrollback, OUT_OF_MEMORY, "Failed to reallocate layouts");
        }
        scrollback->layouts = new_layouts;

        scrollback->capacity = new_capacity;
    }

    // Check if we need to grow the text buffer (need space for text + null terminator)
    if (scrollback->buffer_used + length + 1 > scrollback->buffer_capacity) {
        size_t new_buffer_capacity = scrollback->buffer_capacity * 2;
        // Keep doubling until we have enough space
        while (scrollback->buffer_used + length + 1 > new_buffer_capacity) {
            new_buffer_capacity *= 2;
        }

        char *new_buffer = talloc_realloc_(
            scrollback, scrollback->text_buffer, new_buffer_capacity);
        if (new_buffer == NULL) {     // LCOV_EXCL_BR_LINE
            return ERR(scrollback, OUT_OF_MEMORY, "Failed to reallocate text_buffer");
        }
        scrollback->text_buffer = new_buffer;
        scrollback->buffer_capacity = new_buffer_capacity;
    }

    // Store text offset and length
    scrollback->text_offsets[scrollback->count] = scrollback->buffer_used;
    scrollback->text_lengths[scrollback->count] = length;

    // Copy text to buffer and add null terminator
    if (length > 0) {
        memcpy(scrollback->text_buffer + scrollback->buffer_used, text, length);
        scrollback->buffer_used += length;
    }
    // Add null terminator (even for zero-length lines)
    scrollback->text_buffer[scrollback->buffer_used] = '\0';
    scrollback->buffer_used++;

    // Calculate layout for this line
    ik_line_layout_t layout;
    res_t layout_res = ik_scrollback_calculate_layout(scrollback, text, length,
                                                      scrollback->cached_width, &layout);
    if (is_err(&layout_res)) {     // LCOV_EXCL_BR_LINE
        return layout_res;         // LCOV_EXCL_LINE
    }

    // Store layout
    scrollback->layouts[scrollback->count] = layout;

    // Update totals
    scrollback->total_physical_lines += layout.physical_lines;
    scrollback->count++;

    return OK(scrollback);
}

void ik_scrollback_ensure_layout(ik_scrollback_t *scrollback,
                                 int32_t terminal_width)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE
    assert(terminal_width > 0);  // LCOV_EXCL_BR_LINE

    // If width hasn't changed, nothing to do
    if (terminal_width == scrollback->cached_width) {
        return;
    }

    // Recalculate physical_lines for all lines with new width
    // Use segment_widths to correctly handle embedded newlines
    size_t new_total_physical_lines = 0;
    for (size_t i = 0; i < scrollback->count; i++) {
        size_t segment_count = scrollback->layouts[i].newline_count + 1;
        size_t *seg_widths = scrollback->layouts[i].segment_widths;
        size_t physical_lines = 0;

        // Calculate physical lines by summing rows needed for each segment
        for (size_t seg = 0; seg < segment_count; seg++) {
            if (seg_widths[seg] == 0) {
                physical_lines += 1;  // Empty segment = 1 row
            } else {
                // Calculate rows for this segment: ceil(seg_width / terminal_width)
                physical_lines += (seg_widths[seg] + (size_t)terminal_width - 1) /
                                  (size_t)terminal_width;
            }
        }

        scrollback->layouts[i].physical_lines = physical_lines;
        new_total_physical_lines += physical_lines;
    }

    // Update cached width and total
    scrollback->cached_width = terminal_width;
    scrollback->total_physical_lines = new_total_physical_lines;
}

size_t ik_scrollback_get_line_count(const ik_scrollback_t *scrollback)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE

    return scrollback->count;
}

size_t ik_scrollback_get_total_physical_lines(const ik_scrollback_t *scrollback)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE

    return scrollback->total_physical_lines;
}

res_t ik_scrollback_get_line_text(ik_scrollback_t *scrollback,
                                  size_t line_index,
                                  const char **text_out,
                                  size_t *length_out)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE
    assert(text_out != NULL);    // LCOV_EXCL_BR_LINE
    assert(length_out != NULL);  // LCOV_EXCL_BR_LINE

    // Check bounds
    if (line_index >= scrollback->count) {
        return ERR(scrollback, OUT_OF_RANGE, "Line index %zu out of range (count=%zu)",
                   line_index, scrollback->count);
    }

    // Return pointer into text buffer
    size_t offset = scrollback->text_offsets[line_index];
    size_t length = scrollback->text_lengths[line_index];

    *text_out = scrollback->text_buffer + offset;
    *length_out = length;

    return OK(NULL);
}

res_t ik_scrollback_find_logical_line_at_physical_row(
    ik_scrollback_t *scrollback,
    size_t physical_row,
    size_t *line_index_out,
    size_t *row_offset_out)
{
    assert(scrollback != NULL);       // LCOV_EXCL_BR_LINE
    assert(line_index_out != NULL);   // LCOV_EXCL_BR_LINE
    assert(row_offset_out != NULL);   // LCOV_EXCL_BR_LINE

    // Check if physical_row is out of range
    if (physical_row >= scrollback->total_physical_lines) {
        return ERR(scrollback, OUT_OF_RANGE,
                   "Physical row %zu out of range (total=%zu)",
                   physical_row, scrollback->total_physical_lines);
    }

    // Scan through lines to find which one contains physical_row
    size_t current_row = 0;
    for (size_t i = 0; i < scrollback->count; i++) {
        size_t line_physical_lines = scrollback->layouts[i].physical_lines;

        // Check if physical_row falls within this line
        if (physical_row < current_row + line_physical_lines) {
            *line_index_out = i;
            *row_offset_out = physical_row - current_row;
            return OK(NULL);
        }

        current_row += line_physical_lines;
    }

    // Should never reach here if physical_row is in range
    return ERR(scrollback, OUT_OF_RANGE, "Failed to find line for physical row %zu",
               physical_row);
}

void ik_scrollback_clear(ik_scrollback_t *scrollback)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE

    // Free segment_widths arrays for all lines
    for (size_t i = 0; i < scrollback->count; i++) {
        if (scrollback->layouts[i].segment_widths != NULL) {  // LCOV_EXCL_BR_LINE - Defensive: segment_widths always allocated
            talloc_free(scrollback->layouts[i].segment_widths);
            scrollback->layouts[i].segment_widths = NULL;
        }
    }

    // Reset counters to empty state
    scrollback->count = 0;
    scrollback->buffer_used = 0;
    scrollback->total_physical_lines = 0;

    // Preserve allocated capacity for efficient reuse
    // (no need to free/reallocate arrays)
}

res_t ik_scrollback_copy_from(ik_scrollback_t *dest, const ik_scrollback_t *src)
{
    assert(dest != NULL);  // LCOV_EXCL_BR_LINE
    assert(src != NULL);   // LCOV_EXCL_BR_LINE

    // Copy each line from source to destination
    for (size_t i = 0; i < src->count; i++) {
        const char *text = src->text_buffer + src->text_offsets[i];
        size_t length = src->text_lengths[i];

        res_t res = ik_scrollback_append_line(dest, text, length);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;         // LCOV_EXCL_LINE
        }
    }

    return OK(NULL);
}

res_t ik_scrollback_get_byte_offset_at_display_col(ik_scrollback_t *scrollback,
                                                   size_t line_index,
                                                   size_t display_col,
                                                   size_t *byte_offset_out)
{
    assert(scrollback != NULL);       // LCOV_EXCL_BR_LINE
    assert(byte_offset_out != NULL);  // LCOV_EXCL_BR_LINE

    // Validate line index
    if (line_index >= scrollback->count) {
        return ERR(scrollback, OUT_OF_RANGE,
                   "Line index %zu out of range (count=%zu)",
                   line_index, scrollback->count);
    }

    // Column 0 always starts at byte 0
    if (display_col == 0) {
        *byte_offset_out = 0;
        return OK(NULL);
    }

    // Get line text
    const char *text = scrollback->text_buffer + scrollback->text_offsets[line_index];
    size_t length = scrollback->text_lengths[line_index];

    // Walk through text, tracking display columns
    size_t pos = 0;
    size_t col = 0;

    while (pos < length && col < display_col) {
        // Skip ANSI escape sequences (0 display width)
        size_t skip = ik_ansi_skip_csi(text, length, pos);
        if (skip > 0) {
            pos += skip;
            continue;
        }

        // Decode UTF-8 codepoint
        utf8proc_int32_t cp;
        utf8proc_ssize_t bytes = utf8proc_iterate(
            (const utf8proc_uint8_t *)(text + pos),
            (utf8proc_ssize_t)(length - pos),
            &cp);

        if (bytes <= 0) {  // LCOV_EXCL_BR_LINE
            // LCOV_EXCL_START - Invalid UTF-8 is not produced by our system
            // Invalid UTF-8 - treat as 1 byte, 1 column
            col++;
            pos++;
            continue;
            // LCOV_EXCL_STOP
        }

        // Skip newlines (they don't contribute to display width)
        if (cp == '\n') {
            pos += (size_t)bytes;
            continue;
        }

        // Get character display width
        int32_t width = utf8proc_charwidth(cp);
        if (width > 0) {  // LCOV_EXCL_BR_LINE - Defensive: width typically >0 for printable chars
            col += (size_t)width;
        }

        pos += (size_t)bytes;
    }

    // Skip any ANSI sequences that precede the character at target column
    while (pos < length) {
        size_t skip = ik_ansi_skip_csi(text, length, pos);
        if (skip > 0) {
            pos += skip;
        } else {
            break;
        }
    }

    *byte_offset_out = pos;
    return OK(NULL);
}
