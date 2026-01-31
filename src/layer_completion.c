// Completion layer wrapper
#include "layer_wrappers.h"
#include "completion.h"
#include "commands.h"
#include "panic.h"
#include <assert.h>
#include <string.h>


#include "poison.h"
// Completion layer data
typedef struct {
    ik_completion_t **completion_ptr;  // Raw pointer to completion context pointer (defined in completion.h)
} ik_completion_layer_data_t;

// Completion layer callbacks
static bool completion_is_visible(const ik_layer_t *layer)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    ik_completion_layer_data_t *data = (ik_completion_layer_data_t *)layer->data;
    return *data->completion_ptr != NULL;
}

static size_t completion_get_height(const ik_layer_t *layer, size_t width)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    (void)width;

    ik_completion_layer_data_t *data = (ik_completion_layer_data_t *)layer->data;
    ik_completion_t *comp = *data->completion_ptr;

    if (comp == NULL) {
        return 0;
    }

    return comp->count;
}

static void completion_render(const ik_layer_t *layer,
                              ik_output_buffer_t *output,
                              size_t width,
                              size_t start_row,
                              size_t row_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE
    assert(output != NULL);      // LCOV_EXCL_BR_LINE

    (void)start_row;

    ik_completion_layer_data_t *data = (ik_completion_layer_data_t *)layer->data;
    ik_completion_t *comp = *data->completion_ptr;

    if (comp == NULL || row_count == 0) {  // LCOV_EXCL_BR_LINE - defensive: row_count > 0 ensured by caller
        return;
    }

    // Get command registry to look up descriptions
    size_t cmd_count;
    const ik_command_t *commands = ik_cmd_get_all(&cmd_count);

    // Render each candidate
    size_t candidates_to_render = (row_count < comp->count) ? row_count : comp->count;
    for (size_t i = 0; i < candidates_to_render; i++) {
        const char *candidate = comp->candidates[i];
        bool is_current = (i == comp->current);

        // Find the description for this command
        const char *description = "";
        for (size_t j = 0; j < cmd_count; j++) {  // LCOV_EXCL_BR_LINE - defensive: all candidates have descriptions
            if (strcmp(commands[j].name, candidate) == 0) {
                description = commands[j].description;
                break;
            }
        }

        // Apply ANSI reverse video and bold if this is the current selection
        if (is_current) {
            ik_output_buffer_append(output, "\x1b[7;1m", 6);
        }

        // Render: "  command   description"
        ik_output_buffer_append(output, "  ", 2);
        ik_output_buffer_append(output, candidate, strlen(candidate));
        ik_output_buffer_append(output, "   ", 3);
        ik_output_buffer_append(output, description, strlen(description));

        // Reset ANSI if we applied reverse video
        if (is_current) {
            ik_output_buffer_append(output, "\x1b[0m", 4);
        }

        // Pad to width if needed (to clear any previous content)
        // Note: ANSI codes don't add to visible width
        size_t visible_len = 2 + strlen(candidate) + 3 + strlen(description);
        while (visible_len < width) {
            ik_output_buffer_append(output, " ", 1);
            visible_len++;
        }

        // Add \x1b[K\r\n at end of line (clear to end of line before newline)
        ik_output_buffer_append(output, "\x1b[K\r\n", 5);
    }
}

// Create completion layer
ik_layer_t *ik_completion_layer_create(TALLOC_CTX *ctx,
                                       const char *name,
                                       ik_completion_t **completion_ptr)
{
    assert(ctx != NULL);              // LCOV_EXCL_BR_LINE
    assert(name != NULL);             // LCOV_EXCL_BR_LINE
    assert(completion_ptr != NULL);   // LCOV_EXCL_BR_LINE

    // Allocate completion data
    ik_completion_layer_data_t *data = talloc_zero(ctx, ik_completion_layer_data_t);
    if (data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    data->completion_ptr = completion_ptr;

    // Create layer
    return ik_layer_create(ctx, name, data,
                           completion_is_visible,
                           completion_get_height,
                           completion_render);
}
