## Overview

This file implements the completion layer, which is a visual UI component that displays command completion suggestions to the user. It manages the rendering of a list of available commands with their descriptions, highlighting the currently selected completion candidate using ANSI text styling.

## Code

```
completion_layer_data structure:
  stores a pointer to the active completion context
  (the completion context contains the list of candidates and current selection)

completion_is_visible(layer):
  check if completion context exists and is not null
  return true if completion menu should be displayed

completion_get_height(layer, width):
  retrieve the completion context from layer data
  if completion context is null or doesn't exist:
    return height of 0 (menu not visible)
  otherwise:
    return the number of completion candidates
    (each candidate takes one line of height)

completion_render(layer, output_buffer, width, start_row, row_count):
  retrieve the completion context from layer data

  if completion context doesn't exist or no rows to render:
    return early (nothing to draw)

  get the command registry to look up command descriptions

  determine how many candidates can fit in available rows:
    candidates_to_render = minimum of (available rows, total candidates)

  for each candidate from 0 to candidates_to_render:
    get the candidate name and check if it's the current selection

    find matching command description in registry by name:
      search through all commands
      if name matches, use that command's description
      otherwise use empty string as fallback

    if this candidate is currently selected:
      apply ANSI reverse video (inverted colors) and bold formatting

    render the line: "  " + candidate_name + "   " + description

    if selected, reset ANSI formatting

    pad the line with spaces to fill the terminal width
    (preserves display width while accounting for ANSI codes that don't affect visible width)

    append line ending (carriage return + newline)

ik_completion_layer_create(ctx, name, completion_ptr):
  validate inputs are non-null

  allocate completion layer data structure using memory context

  if allocation fails:
    panic (out of memory)

  store the completion context pointer in the data structure

  create and return a layer with:
    - provided name
    - layer data
    - visibility callback (checks if completion exists)
    - height callback (returns candidate count)
    - render callback (draws candidates with current selection highlighted)
```
