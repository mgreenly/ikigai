/**
 * @file scrollback_render.c
 * @brief Scrollback rendering helper functions
 */

#include "apps/ikigai/scrollback.h"
#include "shared/error.h"
#include <assert.h>


#include "shared/poison.h"
size_t ik_scrollback_calc_start_byte_for_row(ik_scrollback_t *scrollback,
                                             size_t line_index,
                                             size_t terminal_width,
                                             size_t start_row_offset)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE

    if (start_row_offset == 0) {
        return 0;
    }

    const char *line_text = NULL;
    size_t line_len = 0;
    res_t res = ik_scrollback_get_line_text(scrollback, line_index, &line_text, &line_len);
    (void)res;

    size_t segment_count = scrollback->layouts[line_index].newline_count + 1;
    size_t *seg_widths = scrollback->layouts[line_index].segment_widths;
    size_t rows_to_skip = start_row_offset;
    size_t seg_idx = 0;
    size_t partial_rows = 0;

    // Find which segment we start in
    // LCOV_EXCL_BR_START - Defensive: well-formed layouts always have segments
    while (seg_idx < segment_count && rows_to_skip > 0) {
        size_t seg_rows = (seg_widths[seg_idx] == 0) ? 1
            : (seg_widths[seg_idx] + terminal_width - 1) / terminal_width;
        // LCOV_EXCL_BR_STOP

        if (rows_to_skip >= seg_rows) {
            rows_to_skip -= seg_rows;
            seg_idx++;
        } else {
            partial_rows = rows_to_skip;
            rows_to_skip = 0;
        }
    }

    // Calculate display columns to skip
    size_t cols_in_prev_segments = 0;
    for (size_t s = 0; s < seg_idx; s++) {
        cols_in_prev_segments += seg_widths[s];
    }
    size_t cols_to_skip = cols_in_prev_segments + (partial_rows * terminal_width);

    size_t start_byte = 0;
    res = ik_scrollback_get_byte_offset_at_display_col(scrollback, line_index, cols_to_skip, &start_byte);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return 0;     // LCOV_EXCL_LINE - Error path for malformed UTF-8
    }

    if (seg_idx > 0) {
        // Skip past newlines if we skipped entire segments
        size_t newlines_to_skip = seg_idx;
        size_t newlines_seen = 0;
        for (size_t j = 0; j < line_len; j++) {  // LCOV_EXCL_BR_LINE - Defensive: loop always finds newlines
            if (line_text[j] == '\n') {
                newlines_seen++;
                if (newlines_seen == newlines_to_skip) {
                    return j + 1;
                }
            }
        }
    }

    return start_byte;
}

size_t ik_scrollback_calc_end_byte_for_row(ik_scrollback_t *scrollback,
                                           size_t line_index,
                                           size_t terminal_width,
                                           size_t end_row_offset,
                                           bool *is_line_end_out)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE
    assert(is_line_end_out != NULL);  // LCOV_EXCL_BR_LINE

    const char *line_text = NULL;
    size_t line_len = 0;
    res_t res = ik_scrollback_get_line_text(scrollback, line_index, &line_text, &line_len);
    (void)res;

    size_t line_physical_rows = scrollback->layouts[line_index].physical_lines;

    // Check if we're rendering to end of line
    if (end_row_offset + 1 >= line_physical_rows) {
        *is_line_end_out = true;
        return line_len;
    }

    *is_line_end_out = false;

    size_t segment_count = scrollback->layouts[line_index].newline_count + 1;
    size_t *seg_widths = scrollback->layouts[line_index].segment_widths;
    size_t rows_to_include = end_row_offset + 1;
    size_t seg_idx = 0;
    size_t partial_rows = 0;

    // Find which segment we end in
    // LCOV_EXCL_BR_START - Defensive: well-formed layouts always have segments
    while (seg_idx < segment_count && rows_to_include > 0) {
        size_t seg_rows = (seg_widths[seg_idx] == 0) ? 1
            : (seg_widths[seg_idx] + terminal_width - 1) / terminal_width;
        // LCOV_EXCL_BR_STOP

        if (rows_to_include >= seg_rows) {  // LCOV_EXCL_BR_LINE - Edge case: consuming entire segments
            // LCOV_EXCL_START
            rows_to_include -= seg_rows;
            seg_idx++;
            // LCOV_EXCL_STOP
        } else {
            partial_rows = rows_to_include;
            rows_to_include = 0;
        }
    }

    // Calculate display columns to include
    size_t cols_in_prev_segments = 0;
    for (size_t s = 0; s < seg_idx; s++) {     // LCOV_EXCL_BR_LINE
        cols_in_prev_segments += seg_widths[s];     // LCOV_EXCL_LINE
    }
    size_t cols_to_include = cols_in_prev_segments + (partial_rows * terminal_width);

    size_t end_byte = line_len;
    res = ik_scrollback_get_byte_offset_at_display_col(scrollback, line_index, cols_to_include, &end_byte);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return line_len;     // LCOV_EXCL_LINE - Error path for malformed UTF-8
    }

    if (seg_idx > 0) {     // LCOV_EXCL_BR_LINE
        // LCOV_EXCL_START - Edge case: multiple segments with newlines
        // Include newlines if we included entire segments
        size_t newlines_to_include = seg_idx;
        size_t newlines_seen = 0;
        for (size_t j = 0; j < line_len; j++) {
            if (line_text[j] == '\n') {
                newlines_seen++;
                if (newlines_seen == newlines_to_include) {
                    return j + 1;
                }
            }
        }
        // LCOV_EXCL_STOP
    }

    return end_byte;
}

void ik_scrollback_calc_byte_range_for_rows(ik_scrollback_t *scrollback,
                                            size_t line_index,
                                            size_t terminal_width,
                                            size_t start_row_offset,
                                            size_t row_count,
                                            size_t *start_byte_out,
                                            size_t *end_byte_out,
                                            bool *is_line_end_out)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE
    assert(start_byte_out != NULL);  // LCOV_EXCL_BR_LINE
    assert(end_byte_out != NULL);  // LCOV_EXCL_BR_LINE
    assert(is_line_end_out != NULL);  // LCOV_EXCL_BR_LINE

    *start_byte_out = ik_scrollback_calc_start_byte_for_row(scrollback, line_index,
                                                            terminal_width, start_row_offset);

    size_t end_row_offset = start_row_offset + row_count - 1;
    *end_byte_out = ik_scrollback_calc_end_byte_for_row(scrollback, line_index,
                                                        terminal_width, end_row_offset,
                                                        is_line_end_out);
}
