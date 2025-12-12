#include "layer.h"

#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <string.h>

// Create output buffer with initial capacity
ik_output_buffer_t *ik_output_buffer_create(TALLOC_CTX *ctx, size_t initial_capacity)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(initial_capacity > 0); // LCOV_EXCL_BR_LINE

    ik_output_buffer_t *buf = talloc(ctx, ik_output_buffer_t);
    if (buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    buf->data = talloc_array_(buf, sizeof(char), initial_capacity);
    if (buf->data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    buf->size = 0;
    buf->capacity = initial_capacity;

    return buf;
}

// Append bytes to output buffer (grows if needed)
void ik_output_buffer_append(ik_output_buffer_t *buf, const char *data, size_t len)
{
    assert(buf != NULL);                           // LCOV_EXCL_BR_LINE
    assert(data != NULL || len == 0);              // LCOV_EXCL_BR_LINE

    if (len == 0) {
        return;
    }

    // Check if we need to grow the buffer
    size_t required_capacity = buf->size + len;
    if (required_capacity > buf->capacity) {
        // Grow by 1.5x or to required capacity, whichever is larger
        size_t new_capacity = buf->capacity * 3 / 2;
        if (new_capacity < required_capacity) {
            new_capacity = required_capacity;
        }

        char *new_data = talloc_realloc_(buf, buf->data, new_capacity);
        if (new_data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    // Append data
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
}

// Create a layer
ik_layer_t *ik_layer_create(TALLOC_CTX *ctx,
                            const char *name,
                            void *data,
                            ik_layer_is_visible_fn is_visible,
                            ik_layer_get_height_fn get_height,
                            ik_layer_render_fn render)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(name != NULL);       // LCOV_EXCL_BR_LINE
    assert(is_visible != NULL); // LCOV_EXCL_BR_LINE
    assert(get_height != NULL); // LCOV_EXCL_BR_LINE
    assert(render != NULL);     // LCOV_EXCL_BR_LINE

    ik_layer_t *layer = talloc(ctx, ik_layer_t);
    if (layer == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    layer->name = name;
    layer->data = data;
    layer->is_visible = is_visible;
    layer->get_height = get_height;
    layer->render = render;

    return layer;
}

// Create layer cake
ik_layer_cake_t *ik_layer_cake_create(TALLOC_CTX *ctx, size_t viewport_height)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE

    ik_layer_cake_t *cake = talloc(ctx, ik_layer_cake_t);
    if (cake == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    const size_t initial_capacity = 4; // Reasonable starting size
    cake->layers = talloc_array_(cake, sizeof(ik_layer_t *), initial_capacity);
    if (cake->layers == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    cake->layer_count = 0;
    cake->layer_capacity = initial_capacity;
    cake->viewport_row = 0;
    cake->viewport_height = viewport_height;

    return cake;
}

// Add layer to cake (appends to end)
res_t ik_layer_cake_add_layer(ik_layer_cake_t *cake, ik_layer_t *layer)
{
    assert(cake != NULL);   // LCOV_EXCL_BR_LINE
    assert(layer != NULL);  // LCOV_EXCL_BR_LINE

    // Grow array if needed
    if (cake->layer_count >= cake->layer_capacity) {
        size_t new_capacity = cake->layer_capacity * 2;
        ik_layer_t **new_layers = talloc_realloc_(cake, cake->layers,
                                                  sizeof(ik_layer_t *) * new_capacity);
        if (new_layers == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        cake->layers = new_layers;
        cake->layer_capacity = new_capacity;
    }

    cake->layers[cake->layer_count] = layer;
    cake->layer_count++;

    return OK(cake);
}

// Calculate total visible height of all layers
size_t ik_layer_cake_get_total_height(const ik_layer_cake_t *cake, size_t width)
{
    assert(cake != NULL);  // LCOV_EXCL_BR_LINE

    size_t total = 0;
    for (size_t i = 0; i < cake->layer_count; i++) {
        ik_layer_t *layer = cake->layers[i];
        if (layer->is_visible(layer)) {
            total += layer->get_height(layer, width);
        }
    }
    return total;
}

// Render visible portion of cake to output buffer
void ik_layer_cake_render(const ik_layer_cake_t *cake, ik_output_buffer_t *output, size_t width)
{
    assert(cake != NULL);   // LCOV_EXCL_BR_LINE
    assert(output != NULL); // LCOV_EXCL_BR_LINE
    assert(width > 0);      // LCOV_EXCL_BR_LINE

    size_t current_row = 0;
    size_t viewport_end = cake->viewport_row + cake->viewport_height;

    for (size_t i = 0; i < cake->layer_count; i++) {
        ik_layer_t *layer = cake->layers[i];

        if (!layer->is_visible(layer)) {
            continue; // Skip invisible layers
        }

        size_t layer_height = layer->get_height(layer, width);
        size_t layer_end = current_row + layer_height;

        // Check if layer is in viewport
        if (layer_end > cake->viewport_row && current_row < viewport_end) { // LCOV_EXCL_BR_LINE
            // Calculate which portion of the layer to render
            size_t start_row = 0;
            size_t row_count = layer_height;

            // Clip to viewport
            if (current_row < cake->viewport_row) {
                start_row = cake->viewport_row - current_row;
                row_count -= start_row;
            }
            if (layer_end > viewport_end) {
                row_count -= (layer_end - viewport_end);
            }

            // Render this layer's portion
            layer->render(layer, output, width, start_row, row_count);
        }

        current_row = layer_end;

        // Early exit if we've passed the viewport
        if (current_row >= viewport_end) {
            break;
        }
    }
}
