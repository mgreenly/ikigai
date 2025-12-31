## Overview

The separator layer renders a visual separator line across the terminal using Unicode box-drawing characters. It can display navigation context (showing parent, siblings, and child agent nodes) and optional debug information (viewport state, scrollback height, render timing). This layer acts as a wrapper that decorates a single-row separator with contextual metadata about the current agent state in the terminal UI.

## Code

```
TYPES:
  NavigationContext:
    parent_uuid - identifier of parent agent (null if root)
    prev_sibling_uuid - identifier of previous sibling agent (null if none)
    current_uuid - identifier of current agent (required)
    next_sibling_uuid - identifier of next sibling agent (null if none)
    child_count - number of child agents

  DebugInfo:
    viewport_offset - scroll position
    viewport_row - first visible row index
    viewport_height - terminal height in rows
    document_height - total document height
    render_elapsed_us - time elapsed since previous render (microseconds)

  SeparatorLayer:
    visible_ptr - pointer to visibility flag
    debug - debug info pointers (all optional, can be null)
    nav_context - navigation context (optional)


LAYER CALLBACKS:

  is_visible():
    return the current visibility flag

  get_height():
    return 1 (separator is always a single row)

  render(width, start_row, row_count):
    if navigation context is configured:
      format parent indicator
        if parent exists: show truncated parent UUID with up arrow
        otherwise: show dimmed up arrow
      format prev sibling indicator
        if prev sibling exists: show truncated UUID with left arrow
        otherwise: show dimmed left arrow
      format current agent: show truncated UUID in brackets
      format next sibling indicator
        if next sibling exists: show truncated UUID with right arrow
        otherwise: show dimmed right arrow
      format child count indicator
        if children exist: show down arrow and child count
        otherwise: show dimmed down arrow
      assemble navigation string from all indicators

    if debug info is available:
      calculate scrollback rows from document height (doc = sb + input + padding)
      get elapsed render time
      if render time >= 1000 microseconds:
        format debug string with milliseconds: "off=X row=Y h=Z doc=D sb=S t=XXms"
      otherwise:
        format debug string with microseconds: "off=X row=Y h=Z doc=D sb=S t=XXus"

    calculate visual width of info strings (accounting for ANSI escape codes)
    calculate number of separator characters to draw
      separator chars = width - visual width of both info strings

    render separator characters by repeating box-drawing horizontal line
    append navigation context string (if present)
    append debug string (if present)
    append line ending


PUBLIC API:

  create_separator_layer(context, name, visibility_ptr) -> layer:
    validate inputs
    allocate separator layer data structure
    store visibility pointer
    initialize all debug pointers to null
    initialize navigation context to null/zero
    create and return layer with callbacks

  set_debug_info(layer, offset, row, height, doc_height, elapsed_us):
    validate layer
    store pointers to debug data on layer

  set_navigation_context(layer, parent_uuid, prev_uuid, current_uuid, next_uuid, child_count):
    validate layer
    store navigation context on layer
```
