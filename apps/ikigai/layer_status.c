// Status layer wrapper
#include "apps/ikigai/layer_wrappers.h"

#include "apps/ikigai/ansi.h"
#include "apps/ikigai/commands_fork_helpers.h"
#include "apps/ikigai/layer.h"
#include "shared/panic.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>


#include "shared/poison.h"
// Unicode box-drawing character U+2500 (─) in UTF-8: 0xE2 0x94 0x80
#define BOX_DRAWING_LIGHT_HORIZONTAL "\xE2\x94\x80"
#define BOX_DRAWING_LIGHT_HORIZONTAL_LEN 3

// Robot emoji (🤖) in UTF-8: 0xF0 0x9F 0xA4 0x96
#define ROBOT_EMOJI "\xF0\x9F\xA4\x96"

// Status layer data
typedef struct {
    bool *visible_ptr;          // Raw pointer to visibility flag
    char **model_ptr;           // Raw pointer to model string pointer
    int *thinking_level_ptr;    // Raw pointer to thinking level
} ik_status_layer_data_t;

// Status layer callbacks
static bool status_is_visible(const ik_layer_t *layer)
{
    assert(layer != NULL);           // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL);     // LCOV_EXCL_BR_LINE

    ik_status_layer_data_t *data = (ik_status_layer_data_t *)layer->data;
    return *data->visible_ptr;
}

static size_t status_get_height(const ik_layer_t *layer, size_t width)
{
    (void)layer;
    (void)width;
    return 2; // Status layer is always 2 rows (separator + status)
}

static void status_render(const ik_layer_t *layer,
                         ik_output_buffer_t *output,
                         size_t width,
                         size_t start_row,
                         size_t row_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE
    (void)start_row; // Status is only 2 rows, so start_row is always 0
    (void)row_count; // Always render the full status layer

    ik_status_layer_data_t *data = (ik_status_layer_data_t *)layer->data;

    // Row 1: Render separator line
    for (size_t i = 0; i < width; i++) {
        ik_output_buffer_append(output, BOX_DRAWING_LIGHT_HORIZONTAL, BOX_DRAWING_LIGHT_HORIZONTAL_LEN);
    }
    ik_output_buffer_append(output, "\x1b[K\r\n", 5);

    // Row 2: Render status line
    // Build color escape sequence for color 153 (soft blue)
    char color_seq[16];
    size_t color_len = ik_ansi_fg_256(color_seq, sizeof(color_seq), 153);

    // Append robot emoji (default color)
    ik_output_buffer_append(output, ROBOT_EMOJI, 4);
    ik_output_buffer_append(output, " ", 1);

    // Get model and thinking level
    const char *model = *data->model_ptr;
    int thinking_level = *data->thinking_level_ptr;

    if (model == NULL) {
        // No model set - display "(no model)"
        ik_output_buffer_append(output, color_seq, color_len);
        ik_output_buffer_append(output, "(no model)", 10);
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
    } else {
        // Display model/thinking
        const char *thinking_str = thinking_level_to_string(thinking_level);

        // Append colored model/thinking text
        ik_output_buffer_append(output, color_seq, color_len);
        ik_output_buffer_append(output, model, strlen(model));
        ik_output_buffer_append(output, "/", 1);
        ik_output_buffer_append(output, thinking_str, strlen(thinking_str));
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
    }

    // Add \x1b[K\r\n at end of line (clear to end of line before newline)
    ik_output_buffer_append(output, "\x1b[K\r\n", 5);
}

// Create status layer
ik_layer_t *ik_status_layer_create(TALLOC_CTX *ctx,
                                   const char *name,
                                   bool *visible_ptr,
                                   char **model_ptr,
                                   int *thinking_level_ptr)
{
    assert(ctx != NULL);                  // LCOV_EXCL_BR_LINE
    assert(name != NULL);                 // LCOV_EXCL_BR_LINE
    assert(visible_ptr != NULL);          // LCOV_EXCL_BR_LINE
    assert(model_ptr != NULL);            // LCOV_EXCL_BR_LINE
    assert(thinking_level_ptr != NULL);   // LCOV_EXCL_BR_LINE

    // Allocate status data
    ik_status_layer_data_t *data = talloc_zero(ctx, ik_status_layer_data_t);
    if (data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    data->visible_ptr = visible_ptr;
    data->model_ptr = model_ptr;
    data->thinking_level_ptr = thinking_level_ptr;

    // Create layer
    return ik_layer_create(ctx, name, data,
                           status_is_visible,
                           status_get_height,
                           status_render);
}
