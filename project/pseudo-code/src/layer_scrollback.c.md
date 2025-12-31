## Overview

This file implements a layer wrapper that integrates a scrollback buffer into the layered rendering system. The scrollback layer handles visibility checks, height calculations, and text rendering of terminal scrollback content, including proper layout management for variable terminal widths and line wrapping.

## Code

```
struct ScrollbackLayerData:
    scrollback: reference to the scrollback buffer object

function scrollback_is_visible(layer):
    return true (scrollback is always visible)

function scrollback_get_height(layer, width):
    validate layer and its data exist

    retrieve scrollback buffer from layer data

    ensure scrollback layout is current for the given width
    return the total number of physical (wrapped) lines in scrollback

function scrollback_render(layer, output_buffer, width, start_row, row_count):
    validate layer, data, and output buffer exist

    retrieve scrollback buffer from layer data

    ensure layout is up to date for the given width

    if scrollback is empty or no rows to render:
        return immediately

    find which logical line (unwrapped line) corresponds to start_row:
        if start_row is beyond all content:
            return (nothing to render)

    find which logical line corresponds to the last row (start_row + row_count - 1):
        if end row is beyond content:
            use the last logical line
            use the last physical row of that line

    for each logical line from start_line to end_line (inclusive):
        retrieve the text content of the logical line

        calculate which physical rows (wrapped lines) of this logical line to render:
            if this is the first logical line: start from start_row_offset
            if this is the last logical line: render through end_row_offset
            otherwise: render all physical rows of this line

        calculate the byte range within the line text that corresponds to these rows

        copy text bytes from this range to output, converting \n to \r\n

        if we rendered to the end of the logical line:
            append \r\n to output

function scrollback_layer_create(memory_context, layer_name, scrollback):
    validate that context, name, and scrollback all exist

    allocate scrollback layer data structure
    if allocation fails:
        panic (out of memory)

    store reference to scrollback in the layer data

    create and return a new layer with:
        name: provided layer name
        data: the allocated scrollback layer data
        callbacks: visibility, height, and render functions
```
