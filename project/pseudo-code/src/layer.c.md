## Overview

This module implements a layered rendering system for terminal UI. It provides an output buffer for accumulating rendered content, a layer abstraction with visibility and height callbacks, and a layer cake structure that stacks multiple layers, clips them to a viewport window, and renders only the visible portion efficiently.

## Code

```
// Output Buffer - accumulates rendered content with dynamic growth

function create_output_buffer(initial_capacity):
    allocate a buffer structure
    allocate a char array with initial_capacity
    initialize size to 0 and capacity to initial_capacity
    return the buffer

function append_to_output_buffer(buffer, data, length):
    if length is 0:
        return (nothing to append)

    required_size = buffer.size + length

    if required_size exceeds buffer.capacity:
        // Grow buffer by 1.5x or to exact required size, whichever is larger
        new_capacity = buffer.capacity * 1.5
        if new_capacity is still less than required_size:
            new_capacity = required_size

        reallocate buffer's data to new_capacity

    copy data into buffer at current size
    increment buffer size by length


// Layer - represents a renderable component with callbacks

function create_layer(name, data, is_visible, get_height, render):
    validate all parameters are provided

    allocate a layer structure
    store name, data, and callback functions
    return the layer


// Layer Cake - stacks multiple layers with viewport clipping

function create_layer_cake(viewport_height):
    allocate a cake structure
    allocate array for 4 layers (initial capacity)
    initialize layer_count to 0
    store viewport_height and initial viewport position (row 0)
    return the cake

function add_layer_to_cake(cake, layer):
    if layers array is full:
        double the capacity
        reallocate layers array

    append layer to end of layers array
    increment layer_count
    return success

function get_total_visible_height(cake, width):
    total_height = 0

    for each layer in cake:
        if layer is visible:
            total_height += layer's height (calculated for given width)

    return total_height

function render_cake_to_buffer(cake, output_buffer, width):
    // Determine which rows are visible in the viewport
    viewport_start = cake's current viewport row
    viewport_end = viewport_start + viewport height
    current_row = 0

    for each layer in cake:
        // Skip invisible layers
        if layer is not visible:
            continue

        layer_height = layer's height (calculated for given width)
        layer_end = current_row + layer_height

        // Check if this layer overlaps with viewport window
        if layer_end > viewport_start AND current_row < viewport_end:

            // Determine which rows of this layer to render
            render_start_row = 0
            rows_to_render = layer_height

            // Clip top: if layer starts above viewport
            if current_row < viewport_start:
                render_start_row = viewport_start - current_row
                rows_to_render -= render_start_row

            // Clip bottom: if layer extends below viewport
            if layer_end > viewport_end:
                rows_to_render -= (layer_end - viewport_end)

            // Render the clipped portion to output buffer
            call layer's render function with:
                output_buffer
                width
                render_start_row (which row within the layer)
                rows_to_render (how many rows)

        current_row = layer_end

        // Early exit: stop processing if we've rendered past viewport
        if current_row >= viewport_end:
            break
```
