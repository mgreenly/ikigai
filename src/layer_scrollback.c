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

        // Copy line text, converting \n to \r\n
        for (size_t j = 0; j < line_len; j++) {
            if (line_text[j] == '\n') {
                ik_output_buffer_append(output, "\r\n", 2);
            } else {
                ik_output_buffer_append(output, &line_text[j], 1);
            }
        }

        // Add \r\n at end of each line
        ik_output_buffer_append(output, "\r\n", 2);
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
