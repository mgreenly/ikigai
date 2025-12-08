// Separator layer wrapper
#include "layer_wrappers.h"
#include "panic.h"
#include <assert.h>

// Unicode box-drawing character U+2500 (─) in UTF-8: 0xE2 0x94 0x80
#define BOX_DRAWING_LIGHT_HORIZONTAL "\xE2\x94\x80"
#define BOX_DRAWING_LIGHT_HORIZONTAL_LEN 3

// Separator layer data
typedef struct {
    bool *visible_ptr;  // Raw pointer to visibility flag
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
    (void)layer;
    (void)start_row; // Separator is only 1 row, so start_row is always 0
    (void)row_count; // Always render the full separator

    // Render a line of box-drawing characters
    for (size_t i = 0; i < width; i++) {
        ik_output_buffer_append(output, BOX_DRAWING_LIGHT_HORIZONTAL, BOX_DRAWING_LIGHT_HORIZONTAL_LEN);
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

    // Create layer
    return ik_layer_create(ctx, name, data,
                           separator_is_visible,
                           separator_get_height,
                           separator_render);
}
