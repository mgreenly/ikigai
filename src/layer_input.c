// Input layer wrapper
#include "layer_wrappers.h"
#include "panic.h"
#include <assert.h>

// Input layer data
typedef struct {
    bool *visible_ptr;        // Raw pointer to visibility flag
    const char **text_ptr;    // Raw pointer to text pointer
    size_t *text_len_ptr;     // Raw pointer to text length
} ik_input_layer_data_t;

// Input layer callbacks
static bool input_is_visible(const ik_layer_t *layer)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    ik_input_layer_data_t *data = (ik_input_layer_data_t *)layer->data;
    return *data->visible_ptr;
}

static size_t input_get_height(const ik_layer_t *layer, size_t width)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    ik_input_layer_data_t *data = (ik_input_layer_data_t *)layer->data;

    // If no text, input buffer still occupies 1 row (for the cursor)
    if (*data->text_len_ptr == 0) {
        return 1;
    }

    // Calculate physical lines based on text length and width
    // This is a simplified calculation - just count newlines and assume wrapping
    const char *text = *data->text_ptr;
    size_t text_len = *data->text_len_ptr;

    size_t physical_lines = 1;  // Start with 1 line
    size_t current_col = 0;

    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            physical_lines++;
            current_col = 0;
        } else {
            current_col++;
            if (current_col >= width) {
                physical_lines++;
                current_col = 0;
            }
        }
    }

    return physical_lines;
}

static void input_render(const ik_layer_t *layer,
                         ik_output_buffer_t *output,
                         size_t width,
                         size_t start_row,
                         size_t row_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE
    assert(output != NULL);      // LCOV_EXCL_BR_LINE

    (void)width;
    (void)start_row; // TODO: Handle partial rendering
    (void)row_count;

    ik_input_layer_data_t *data = (ik_input_layer_data_t *)layer->data;
    const char *text = *data->text_ptr;
    size_t text_len = *data->text_len_ptr;

    // Empty input: output blank line to reserve cursor space
    if (text_len == 0) {
        ik_output_buffer_append(output, "\r\n", 2);
        return;
    }

    // Render input text with \n to \r\n conversion
    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            ik_output_buffer_append(output, "\r\n", 2);
        } else {
            ik_output_buffer_append(output, &text[i], 1);
        }
    }

    // Add trailing \r\n if text doesn't end with newline
    // (text ending with \n already converted to \r\n above)
    if (text[text_len - 1] != '\n') {
        ik_output_buffer_append(output, "\r\n", 2);
    }
}

// Create input layer
ik_layer_t *ik_input_layer_create(TALLOC_CTX *ctx,
                                  const char *name,
                                  bool *visible_ptr,
                                  const char **text_ptr,
                                  size_t *text_len_ptr)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(name != NULL);         // LCOV_EXCL_BR_LINE
    assert(visible_ptr != NULL);  // LCOV_EXCL_BR_LINE
    assert(text_ptr != NULL);     // LCOV_EXCL_BR_LINE
    assert(text_len_ptr != NULL); // LCOV_EXCL_BR_LINE

    // Allocate input data
    ik_input_layer_data_t *data = talloc(ctx, ik_input_layer_data_t);
    if (data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    data->visible_ptr = visible_ptr;
    data->text_ptr = text_ptr;
    data->text_len_ptr = text_len_ptr;

    // Create layer
    return ik_layer_create(ctx, name, data,
                           input_is_visible,
                           input_get_height,
                           input_render);
}
