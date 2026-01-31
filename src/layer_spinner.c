// Spinner layer wrapper and spinner utilities
#include "layer_wrappers.h"
#include "panic.h"
#include <assert.h>
#include <string.h>


#include "poison.h"
// Spinner animation frames
static const char *SPINNER_FRAMES[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
static const size_t SPINNER_FRAME_COUNT = 10;

// Get current spinner frame string
const char *ik_spinner_get_frame(const ik_spinner_state_t *state)
{
    assert(state != NULL);  // LCOV_EXCL_BR_LINE
    return SPINNER_FRAMES[state->frame_index % SPINNER_FRAME_COUNT];
}

// Advance to next spinner frame (cycles through animation)
void ik_spinner_advance(ik_spinner_state_t *state)
{
    assert(state != NULL);  // LCOV_EXCL_BR_LINE
    state->frame_index = (state->frame_index + 1) % SPINNER_FRAME_COUNT;
}

// Spinner layer data
typedef struct {
    ik_spinner_state_t *state;  // Borrowed pointer to spinner state
} ik_spinner_layer_data_t;

// Spinner layer callbacks
static bool spinner_is_visible(const ik_layer_t *layer)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    ik_spinner_layer_data_t *data = (ik_spinner_layer_data_t *)layer->data;
    return data->state->visible;
}

static size_t spinner_get_height(const ik_layer_t *layer, size_t width)
{
    (void)layer;
    (void)width;
    return 1; // Spinner is always 1 row when visible
}

static void spinner_render(const ik_layer_t *layer,
                           ik_output_buffer_t *output,
                           size_t width,
                           size_t start_row,
                           size_t row_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE
    assert(output != NULL);      // LCOV_EXCL_BR_LINE

    (void)width;
    (void)start_row; // Spinner is only 1 row, so start_row is always 0
    (void)row_count; // Always render the full spinner

    ik_spinner_layer_data_t *data = (ik_spinner_layer_data_t *)layer->data;
    const char *frame = ik_spinner_get_frame(data->state);

    // Render spinner: "<frame> Waiting for response..."
    ik_output_buffer_append(output, frame, strlen(frame));
    ik_output_buffer_append(output, " Waiting for response...", 24);

    // Add \x1b[K\r\n at end of line (clear to end of line before newline)
    ik_output_buffer_append(output, "\x1b[K\r\n", 5);
}

// Create spinner layer
ik_layer_t *ik_spinner_layer_create(TALLOC_CTX *ctx,
                                    const char *name,
                                    ik_spinner_state_t *state)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(name != NULL);      // LCOV_EXCL_BR_LINE
    assert(state != NULL);     // LCOV_EXCL_BR_LINE

    // Allocate spinner data
    ik_spinner_layer_data_t *data = talloc_zero(ctx, ik_spinner_layer_data_t);
    if (data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    data->state = state;

    // Create layer
    return ik_layer_create(ctx, name, data,
                           spinner_is_visible,
                           spinner_get_height,
                           spinner_render);
}
