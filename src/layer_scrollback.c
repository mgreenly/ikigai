// Scrollback layer wrapper
#include "layer_wrappers.h"
#include "scrollback.h"
#include "panic.h"
#include <assert.h>

// Scrollback layer data
typedef struct {
    ik_scrollback_t *scrollback;  // Borrowed pointer to scrollback buffer
} ik_scrollback_layer_data_t;

// Scrollback layer callbacks
static bool scrollback_is_visible(const ik_layer_t *layer)
{
    (void)layer;
    return true; // Scrollback is always visible
}

static size_t scrollback_get_height(const ik_layer_t *layer, size_t width)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    ik_scrollback_layer_data_t *data = (ik_scrollback_layer_data_t *)layer->data;

    // Ensure layout is up to date for this width
    ik_scrollback_ensure_layout(data->scrollback, (int32_t)width);

    return ik_scrollback_get_total_physical_lines(data->scrollback);
}

static void scrollback_render(const ik_layer_t *layer,
                              ik_output_buffer_t *output,
                              size_t width,
                              size_t start_row,
                              size_t row_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE
    assert(output != NULL);      // LCOV_EXCL_BR_LINE

    ik_scrollback_layer_data_t *data = (ik_scrollback_layer_data_t *)layer->data;
    ik_scrollback_t *scrollback = data->scrollback;

    // Ensure layout is up to date
    ik_scrollback_ensure_layout(scrollback, (int32_t)width);

    // Handle empty scrollback
    size_t total_lines = ik_scrollback_get_line_count(scrollback);
    if (total_lines == 0 || row_count == 0) {
        return;
    }

    // Find which logical lines correspond to the requested physical rows
    size_t start_line_idx, start_row_offset;
    res_t res = ik_scrollback_find_logical_line_at_physical_row(scrollback, start_row,
                                                                &start_line_idx, &start_row_offset);
    if (!is_ok(&res)) {
        // start_row is beyond the scrollback content
        return;
    }

    size_t end_physical_row = start_row + row_count;
    size_t end_line_idx, end_row_offset;

    // Find the last logical line (end_physical_row - 1 since we want inclusive range)
    // end_physical_row is always > 0 since row_count > 0 (checked above)
    assert(end_physical_row > 0);  // LCOV_EXCL_BR_LINE
    res = ik_scrollback_find_logical_line_at_physical_row(scrollback, end_physical_row - 1,
                                                          &end_line_idx, &end_row_offset);
    if (!is_ok(&res)) {
        // End row is beyond scrollback, render to the last line
        end_line_idx = total_lines - 1;
        // For end_row_offset, use last row of last line
        end_row_offset = scrollback->layouts[end_line_idx].physical_lines - 1;
    }

    // Render logical lines from start_line_idx to end_line_idx (inclusive)
    // end_line_idx is guaranteed to be < total_lines, so the loop is bounded
    assert(end_line_idx < total_lines);  // LCOV_EXCL_BR_LINE
    for (size_t i = start_line_idx; i <= end_line_idx; i++) {
        const char *line_text = NULL;
        size_t line_len = 0;
        // This cannot fail since i < total_lines (guaranteed by loop bounds)
        res = ik_scrollback_get_line_text(scrollback, i, &line_text, &line_len);
        (void)res;  // Suppress unused variable warning

        // Calculate byte range to render for this line
        size_t render_start = 0;
        size_t render_end = line_len;
        bool is_line_end = true;  // Are we rendering to end of logical line?

        // For first line: skip start_row_offset worth of physical rows
        // Use segment_widths to handle embedded newlines correctly
        if (i == start_line_idx && start_row_offset > 0) {
            size_t rows_to_skip = start_row_offset;
            size_t segment_count = scrollback->layouts[i].newline_count + 1;
            size_t *seg_widths = scrollback->layouts[i].segment_widths;
            size_t seg_idx = 0;
            size_t partial_rows = 0;

            // Find which segment we start in
            while (seg_idx < segment_count && rows_to_skip > 0) {
                size_t seg_rows = (seg_widths[seg_idx] == 0) ? 1
                    : (seg_widths[seg_idx] + width - 1) / width;

                if (rows_to_skip >= seg_rows) {
                    // Skip entire segment
                    rows_to_skip -= seg_rows;
                    seg_idx++;  // Move to next segment
                } else {
                    // Start partway through this segment
                    partial_rows = rows_to_skip;
                    rows_to_skip = 0;
                    // Don't increment seg_idx - we're starting within this segment
                }
            }

            // Calculate display columns to skip within the target segment
            size_t cols_in_prev_segments = 0;
            for (size_t s = 0; s < seg_idx; s++) {
                cols_in_prev_segments += seg_widths[s];
            }
            size_t cols_to_skip = cols_in_prev_segments + (partial_rows * width);

            res = ik_scrollback_get_byte_offset_at_display_col(scrollback, i, cols_to_skip, &render_start);
            if (is_err(&res)) {
                render_start = 0;
            } else if (seg_idx > 0) {
                // If we skipped entire segments, we need to skip past their newlines too
                // Walk through the text to find the seg_idx-th newline
                size_t newlines_to_skip = seg_idx;
                size_t newlines_seen = 0;
                for (size_t j = 0; j < line_len; j++) {
                    if (line_text[j] == '\n') {
                        newlines_seen++;
                        if (newlines_seen == newlines_to_skip) {
                            render_start = j + 1;  // Start after this newline
                            break;
                        }
                    }
                }
            }
        }

        // For last line: only render up to (end_row_offset + 1) physical rows
        // Use segment_widths to handle embedded newlines correctly
        if (i == end_line_idx) {
            size_t line_physical_rows = scrollback->layouts[i].physical_lines;
            // Are we stopping before end of this logical line?
            if (end_row_offset + 1 < line_physical_rows) {
                size_t rows_to_include = end_row_offset + 1;
                size_t segment_count = scrollback->layouts[i].newline_count + 1;
                size_t *seg_widths = scrollback->layouts[i].segment_widths;
                size_t seg_idx = 0;
                size_t partial_rows = 0;

                // Find which segment we end in
                while (seg_idx < segment_count && rows_to_include > 0) {
                    size_t seg_rows = (seg_widths[seg_idx] == 0) ? 1
                        : (seg_widths[seg_idx] + width - 1) / width;

                    if (rows_to_include >= seg_rows) {
                        // Include entire segment
                        rows_to_include -= seg_rows;
                        seg_idx++;  // Move to next segment
                    } else {
                        // End partway through this segment
                        partial_rows = rows_to_include;
                        rows_to_include = 0;
                        // Don't increment seg_idx - we're ending within this segment
                    }
                }

                // Calculate display columns to include within the target segment
                size_t cols_in_prev_segments = 0;
                for (size_t s = 0; s < seg_idx; s++) {
                    cols_in_prev_segments += seg_widths[s];
                }
                size_t cols_to_include = cols_in_prev_segments + (partial_rows * width);

                res = ik_scrollback_get_byte_offset_at_display_col(scrollback, i, cols_to_include, &render_end);
                if (is_err(&res)) {
                    render_end = line_len;
                } else if (seg_idx > 0) {
                    // If we included entire segments, we need to include their newlines too
                    // Walk through the text to find the seg_idx-th newline
                    size_t newlines_to_include = seg_idx;
                    size_t newlines_seen = 0;
                    for (size_t j = 0; j < line_len; j++) {
                        if (line_text[j] == '\n') {
                            newlines_seen++;
                            if (newlines_seen == newlines_to_include) {
                                render_end = j + 1;  // End after this newline
                                break;
                            }
                        }
                    }
                }
                is_line_end = false;  // We're stopping mid-line
            }
        }

        // Copy line text from render_start to render_end, converting \n to \r\n
        for (size_t j = render_start; j < render_end; j++) {
            if (line_text[j] == '\n') {
                ik_output_buffer_append(output, "\r\n", 2);
            } else {
                ik_output_buffer_append(output, &line_text[j], 1);
            }
        }

        // Add \r\n only if we rendered to end of logical line
        if (is_line_end) {
            ik_output_buffer_append(output, "\r\n", 2);
        }
    }
}

// Create scrollback layer
ik_layer_t *ik_scrollback_layer_create(TALLOC_CTX *ctx,
                                       const char *name,
                                       ik_scrollback_t *scrollback)
{
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(name != NULL);        // LCOV_EXCL_BR_LINE
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate scrollback data
    ik_scrollback_layer_data_t *data = talloc(ctx, ik_scrollback_layer_data_t);
    if (data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    data->scrollback = scrollback;

    // Create layer
    return ik_layer_create(ctx, name, data,
                           scrollback_is_visible,
                           scrollback_get_height,
                           scrollback_render);
}
