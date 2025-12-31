## Overview

This file implements viewport and scrolling actions for the REPL interface. It handles user-initiated scrolling operations (page up/down and mouse wheel scrolling) and calculates viewport bounds based on the scrollback buffer and input area dimensions. These functions allow users to navigate through historical content while maintaining the current input line.

## Code

```
function calculate_max_viewport_offset(repl):
    ensure scrollback layout is computed with current terminal width
    ensure input buffer layout is computed with current terminal width

    get total physical lines in scrollback
    get physical lines needed for input buffer
    if input buffer has no lines, display it as 1 line

    calculate document height as:
        scrollback lines + separator line + input buffer display lines + bottom separator line

    if document height exceeds terminal height:
        return how many lines can scroll (document height - terminal height)
    else:
        return 0 (no scrolling possible, content fits on screen)

function handle_page_up_action(repl):
    validate repl context exists

    get maximum allowed viewport offset
    calculate new offset by adding terminal screen height to current offset
    clamp new offset to maximum (don't scroll past start of document)

    update viewport position
    return success

function handle_page_down_action(repl):
    validate repl context exists

    if viewport is already scrolled down by at least one screen height:
        scroll up by one terminal screen height
    else:
        reset viewport to top (offset 0)

    return success

function handle_scroll_up_action(repl):
    validate repl context exists

    get maximum allowed viewport offset
    calculate new offset by adding 3 lines to current offset
    clamp new offset to maximum (don't scroll past start of document)

    update viewport position
    return success

function handle_scroll_down_action(repl):
    validate repl context exists

    if viewport is already scrolled down by at least 3 lines:
        scroll up by 3 lines
    else:
        reset viewport to top (offset 0)

    return success
```
