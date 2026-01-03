## Overview

This file implements the input layer wrapper, a visual layer component that displays user-entered text with word wrapping and newline handling. It manages visibility state, calculates display height based on text and terminal width, and renders the input buffer to the output stream with proper line-ending conversion.

## Code

```
struct InputLayerData:
    visible flag (reference to external boolean)
    text pointer (reference to external string)
    text length (reference to external size)


function input_is_visible(layer):
    return whether the input layer should be displayed


function input_get_height(layer, width):
    if no text in input:
        return 1 (reserve one row for the cursor)

    physical_lines = 1
    current column position = 0

    for each character in text:
        if character is newline:
            increment physical_lines
            reset current column position to 0
        else:
            increment current column position
            if current column exceeds display width:
                increment physical_lines
                reset current column position

    return total physical_lines needed to display the text


function input_render(layer, output, width, start_row, row_count):
    if no text in input:
        output blank line to reserve space for cursor
        return

    for each character in text:
        if character is newline:
            output carriage return + newline (\r\n)
        else:
            output the character

    if text does not end with newline:
        output carriage return + newline to terminate the line


function ik_input_layer_create(context, name, visible_ptr, text_ptr, text_len_ptr):
    allocate input layer data structure in the given memory context
    if allocation fails:
        panic with out-of-memory error

    store references to:
        the visibility flag
        the text pointer
        the text length value

    create and return a new layer with:
        the allocated data
        visibility callback
        height calculation callback
        render callback
```
