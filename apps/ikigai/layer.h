#ifndef IK_LAYER_H
#define IK_LAYER_H

#include "shared/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <talloc.h>

// Forward declarations
typedef struct ik_layer ik_layer_t;
typedef struct ik_output_buffer ik_output_buffer_t;

// Output buffer for accumulating rendered content
// Wraps a char buffer with dynamic growth
struct ik_output_buffer {
    char *data;          // Buffer content
    size_t size;         // Current size (bytes used)
    size_t capacity;     // Total capacity (bytes allocated)
};

// Create output buffer with initial capacity
ik_output_buffer_t *ik_output_buffer_create(TALLOC_CTX *ctx, size_t initial_capacity);

// Append bytes to output buffer (grows if needed)
void ik_output_buffer_append(ik_output_buffer_t *buf, const char *data, size_t len);

// Layer function signatures
typedef bool (*ik_layer_is_visible_fn)(const ik_layer_t *layer);
typedef size_t (*ik_layer_get_height_fn)(const ik_layer_t *layer, size_t width);
typedef void (*ik_layer_render_fn)(const ik_layer_t *layer,
                                   ik_output_buffer_t *output,
                                   size_t width,
                                   size_t start_row,
                                   size_t row_count);

// Layer structure
struct ik_layer {
    const char *name;                  // Layer name (e.g., "scrollback", "spinner", "separator", "input")
    void *data;                        // Layer-specific context
    ik_layer_is_visible_fn is_visible; // Returns true if layer should be rendered
    ik_layer_get_height_fn get_height; // Returns layer height in rows
    ik_layer_render_fn render;         // Renders layer content to output buffer
};

// Create a layer
// Returns newly allocated layer (never returns NULL - PANICs on OOM)
ik_layer_t *ik_layer_create(TALLOC_CTX *ctx,
                            const char *name,
                            void *data,
                            ik_layer_is_visible_fn is_visible,
                            ik_layer_get_height_fn get_height,
                            ik_layer_render_fn render);

// Layer cake - manages ordered collection of layers
typedef struct {
    ik_layer_t **layers;    // Ordered array (top to bottom)
    size_t layer_count;
    size_t layer_capacity;
    size_t viewport_row;    // Current scroll position
    size_t viewport_height; // Terminal height
} ik_layer_cake_t;

// Create layer cake
// Returns newly allocated layer cake (never returns NULL - PANICs on OOM)
ik_layer_cake_t *ik_layer_cake_create(TALLOC_CTX *ctx, size_t viewport_height);

// Add layer to cake (appends to end)
res_t ik_layer_cake_add_layer(ik_layer_cake_t *cake, ik_layer_t *layer);

// Calculate total visible height of all layers
size_t ik_layer_cake_get_total_height(const ik_layer_cake_t *cake, size_t width);

// Render visible portion of cake to output buffer
void ik_layer_cake_render(const ik_layer_cake_t *cake, ik_output_buffer_t *output, size_t width);

#endif // IK_LAYER_H
