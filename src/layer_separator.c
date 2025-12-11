// Separator layer wrapper
#include "layer_wrappers.h"
#include "panic.h"
#include "wrapper.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

// Unicode box-drawing character U+2500 (─) in UTF-8: 0xE2 0x94 0x80
#define BOX_DRAWING_LIGHT_HORIZONTAL "\xE2\x94\x80"
#define BOX_DRAWING_LIGHT_HORIZONTAL_LEN 3

// Debug info pointers (optional - can be NULL)
typedef struct {
    size_t *viewport_offset;     // Scroll offset
    size_t *viewport_row;        // First visible row
    size_t *viewport_height;     // Terminal rows
    size_t *document_height;     // Total document height
    uint64_t *render_elapsed_us; // Elapsed time from previous render
} ik_separator_debug_t;

// Separator layer data
typedef struct {
    bool *visible_ptr;           // Raw pointer to visibility flag
    ik_separator_debug_t debug;  // Debug info pointers (all can be NULL)
} ik_separator_layer_data_t;

// Separator layer callbacks
static bool separator_is_visible(const ik_layer_t *layer)
{
    assert(layer != NULL);           // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL);     // LCOV_EXCL_BR_LINE

    ik_separator_layer_data_t *data = (ik_separator_layer_data_t *)layer->data;
    return *data->visible_ptr;
}

static size_t separator_get_height(const ik_layer_t *layer, size_t width)
{
    (void)layer;
    (void)width;
    return 1; // Separator is always 1 row
}

static void separator_render(const ik_layer_t *layer,
                             ik_output_buffer_t *output,
                             size_t width,
                             size_t start_row,
                             size_t row_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE
    (void)start_row; // Separator is only 1 row, so start_row is always 0
    (void)row_count; // Always render the full separator

    ik_separator_layer_data_t *data = (ik_separator_layer_data_t *)layer->data;

    // Build debug string if debug info available
    char debug_str[128] = "";
    size_t debug_len = 0;

    if (data->debug.viewport_offset != NULL) {
        // Calculate scrollback rows from doc height: doc = sb + 1 + input(1) + 1
        size_t sb_rows = 0;
        if (data->debug.document_height && *data->debug.document_height >= 3) {
            sb_rows = *data->debug.document_height - 3;
        }
        // Get elapsed time from previous render
        uint64_t render_us = 0;
        if (data->debug.render_elapsed_us) {
            render_us = *data->debug.render_elapsed_us;
        }
        // Format render time: show in ms if >= 1000us, otherwise us
        if (render_us >= 1000) {
            debug_len = (size_t)snprintf(debug_str, sizeof(debug_str),
                " off=%zu row=%zu h=%zu doc=%zu sb=%zu t=%.1fms ",
                data->debug.viewport_offset ? *data->debug.viewport_offset : 0,
                data->debug.viewport_row ? *data->debug.viewport_row : 0,
                data->debug.viewport_height ? *data->debug.viewport_height : 0,
                data->debug.document_height ? *data->debug.document_height : 0,
                sb_rows,
                (double)render_us / 1000.0);
        } else {
            debug_len = (size_t)snprintf(debug_str, sizeof(debug_str),
                " off=%zu row=%zu h=%zu doc=%zu sb=%zu t=%" PRIu64 "us ",
                data->debug.viewport_offset ? *data->debug.viewport_offset : 0,
                data->debug.viewport_row ? *data->debug.viewport_row : 0,
                data->debug.viewport_height ? *data->debug.viewport_height : 0,
                data->debug.document_height ? *data->debug.document_height : 0,
                sb_rows,
                render_us);
        }
    }

    // Calculate how many separator chars to draw (leave room for debug)
    size_t sep_chars = width;
    if (debug_len > 0 && debug_len < width) {
        sep_chars = width - debug_len;
    }

    // Render separator chars
    for (size_t i = 0; i < sep_chars; i++) {
        ik_output_buffer_append(output, BOX_DRAWING_LIGHT_HORIZONTAL, BOX_DRAWING_LIGHT_HORIZONTAL_LEN);
    }

    // Append debug string if present
    if (debug_len > 0) {
        ik_output_buffer_append(output, debug_str, debug_len);
    }

    // Add \r\n at end of line
    ik_output_buffer_append(output, "\r\n", 2);
}

// Create separator layer
ik_layer_t *ik_separator_layer_create(TALLOC_CTX *ctx,
                                      const char *name,
                                      bool *visible_ptr)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(name != NULL);         // LCOV_EXCL_BR_LINE
    assert(visible_ptr != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate separator data
    ik_separator_layer_data_t *data = talloc(ctx, ik_separator_layer_data_t);
    if (data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    data->visible_ptr = visible_ptr;
    // Initialize debug pointers to NULL (no debug info by default)
    data->debug.viewport_offset = NULL;
    data->debug.viewport_row = NULL;
    data->debug.viewport_height = NULL;
    data->debug.document_height = NULL;
    data->debug.render_elapsed_us = NULL;

    // Create layer
    return ik_layer_create(ctx, name, data,
                           separator_is_visible,
                           separator_get_height,
                           separator_render);
}

// Set debug info pointers on separator layer
void ik_separator_layer_set_debug(ik_layer_t *layer,
                                  size_t *viewport_offset,
                                  size_t *viewport_row,
                                  size_t *viewport_height,
                                  size_t *document_height,
                                  uint64_t *render_elapsed_us)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    ik_separator_layer_data_t *data = (ik_separator_layer_data_t *)layer->data;
    data->debug.viewport_offset = viewport_offset;
    data->debug.viewport_row = viewport_row;
    data->debug.viewport_height = viewport_height;
    data->debug.document_height = document_height;
    data->debug.render_elapsed_us = render_elapsed_us;
}
