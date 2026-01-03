/**
 * @file scrollback_layout.c
 * @brief Layout calculation for scrollback lines
 */

#include "scrollback_layout.h"

#include "ansi.h"
#include "error.h"
#include "panic.h"
#include "scrollback_utils.h"
#include "wrapper.h"

#include <assert.h>
#include <talloc.h>
#include <utf8proc.h>

res_t ik_scrollback_calculate_layout(void *parent,
                                     const char *text,
                                     size_t length,
                                     int32_t terminal_width,
                                     ik_line_layout_t *layout_out)
{
    assert(parent != NULL);      // LCOV_EXCL_BR_LINE
    assert(layout_out != NULL);  // LCOV_EXCL_BR_LINE

    // First pass: count newlines to allocate segment_widths array
    size_t newline_count = ik_scrollback_count_newlines(text, length);

    // Allocate segment_widths array (newline_count + 1 segments)
    size_t segment_count = newline_count + 1;
    size_t *segment_widths = talloc_array_(parent, sizeof(size_t), segment_count);
    if (segment_widths == NULL) {     // LCOV_EXCL_BR_LINE
        return ERR(parent, OUT_OF_MEMORY, "Failed to allocate segment_widths");  // LCOV_EXCL_LINE
    }

    // Second pass: calculate display width and physical lines by scanning UTF-8
    // Handle newlines: each newline starts a new segment
    size_t physical_lines = 0;
    size_t line_width = 0;
    size_t current_segment = 0;
    size_t pos = 0;
    bool has_any_content = false;  // Track if we've seen any non-newline characters
    bool ends_with_newline = false;  // Track if last character was \n

    while (pos < length) {
        // Skip ANSI escape sequences
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

        if (bytes <= 0) {
            // Invalid UTF-8 - treat as 1 column per byte
            line_width++;
            has_any_content = true;
            ends_with_newline = false;
            pos++;
            continue;
        }

        // Handle newlines
        if (cp == '\n') {
            // Finalize current segment
            segment_widths[current_segment] = line_width;
            if (line_width == 0) {
                physical_lines += 1;  // Empty segment takes 1 row
            } else {
                // Calculate rows for this segment: ceil(line_width / terminal_width)
                size_t line_rows = (line_width + (size_t)terminal_width - 1) /
                                   (size_t)terminal_width;
                physical_lines += line_rows;
            }
            // Start new segment
            current_segment++;
            line_width = 0;
            ends_with_newline = true;
            pos += (size_t)bytes;
            continue;
        }

        // Get display width (may be negative for control chars)
        int32_t width = utf8proc_charwidth(cp);
        if (width > 0) {
            line_width += (size_t)width;
        }
        has_any_content = true;
        ends_with_newline = false;

        pos += (size_t)bytes;
    }

    // Finalize last segment (or only segment if no newlines)
    segment_widths[current_segment] = line_width;
    if (line_width == 0 && physical_lines == 0) {
        physical_lines = 1;  // Empty line still takes 1 row
    } else if (line_width > 0) {
        // Calculate rows for last segment: ceil(line_width / terminal_width)
        size_t line_rows = (line_width + (size_t)terminal_width - 1) /
                           (size_t)terminal_width;
        physical_lines += line_rows;
    } else if (ends_with_newline && has_any_content) {
        // Trailing empty segment after content that ended with newline
        // (line_width == 0 && physical_lines > 0 at this point)
        physical_lines += 1;
    }

    // Calculate total display width for the layout (sum of all segment widths)
    size_t display_width = ik_scrollback_calculate_display_width(text, length);

    // Store layout
    layout_out->display_width = display_width;
    layout_out->physical_lines = physical_lines;
    layout_out->newline_count = newline_count;
    layout_out->segment_widths = segment_widths;

    return OK(parent);
}
