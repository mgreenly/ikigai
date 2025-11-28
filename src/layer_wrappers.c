// Layer wrappers for existing UI components (scrollback, separator, input)
#include "layer_wrappers.h"
#include "scrollback.h"
#include "panic.h"
#include <assert.h>

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

    // Render a line of dashes
    for (size_t i = 0; i < width; i++) {
        ik_output_buffer_append(output, "-", 1);
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

    // Render input text with \n to \r\n conversion
    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            ik_output_buffer_append(output, "\r\n", 2);
        } else {
            ik_output_buffer_append(output, &text[i], 1);
        }
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

// Spinner animation frames
static const char SPINNER_FRAMES[] = {'|', '/', '-', '\\'};
static const size_t SPINNER_FRAME_COUNT = 4;

// Get current spinner frame character
char ik_spinner_get_frame(const ik_spinner_state_t *state)
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
    char frame = ik_spinner_get_frame(data->state);

    // Render spinner: "[<frame>] Waiting for response..."
    ik_output_buffer_append(output, "[", 1);
    ik_output_buffer_append(output, &frame, 1);
    ik_output_buffer_append(output, "] Waiting for response...", 25);

    // Add \r\n at end of line
    ik_output_buffer_append(output, "\r\n", 2);
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
    ik_spinner_layer_data_t *data = talloc(ctx, ik_spinner_layer_data_t);
    if (data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    data->state = state;

    // Create layer
    return ik_layer_create(ctx, name, data,
                           spinner_is_visible,
                           spinner_get_height,
                           spinner_render);
}
