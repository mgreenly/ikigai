## Overview

The spinner layer provides animated visual feedback during asynchronous operations. It manages a rotating animation sequence (|, /, -, \) and integrates with the layer rendering system to display "Waiting for response..." messages with the current animation frame.

## Code

```
spinner animation frames: ['|', '/', '-', '\']

function get_current_frame(spinner_state):
    return the character at the current frame index, cycling through the 4 frames

function advance_frame(spinner_state):
    increment the frame index
    wrap around to 0 when reaching the end of the sequence

function is_spinner_visible(layer):
    validate layer and its data exist
    return whether the spinner is currently visible

function get_spinner_height(layer, width):
    return 1 (spinner always occupies exactly one line of output)

function render_spinner(layer, output_buffer, width, start_row, row_count):
    validate layer, its data, and output buffer exist

    get the current animation frame character from the spinner state

    write to output: "[<frame>] Waiting for response..."
    append line ending to output

function create_spinner_layer(context, layer_name, spinner_state):
    validate context, layer_name, and spinner_state all exist

    allocate memory for the spinner layer data structure
    if memory allocation fails: panic

    store a reference to the spinner state in the layer data

    create and return a new layer with:
        - the layer name and data
        - callbacks for visibility check, height calculation, and rendering
```
