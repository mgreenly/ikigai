## Overview

Processes keyboard and scroll input actions in the REPL. Routes different input types (characters, navigation keys, control keys, arrow keys, etc.) to appropriate handlers that update the input buffer, history, scrollback, and completion state. Also manages scroll detection to distinguish between keyboard arrow navigation and mouse wheel scrolling.

## Code

```
function append_multiline_to_scrollback(scrollback, text):
    if text is empty:
        return

    iterate through text by lines:
        find each newline character (or end of text)
        extract the line from start to newline

        if line is non-empty or there are more characters:
            append line to scrollback

        advance start position past the newline


function process_scroll_detection(repl, input_action):
    validate inputs exist and action is arrow up/down

    if no scroll detector configured:
        return success (let caller handle as normal arrow)

    get current time in milliseconds

    send arrow event to scroll detector with current time

    based on detector result:
        if detected upward scroll:
            handle scroll up
        if detected downward scroll:
            handle scroll down
        if detected normal arrow key:
            return success (caller handles as normal arrow)
        if buffering more events:
            signal handled (don't process further)
        if absorbed in a burst:
            signal handled (don't process further)


function flush_pending_scroll_arrow(repl, input_action):
    validate inputs exist and action is not arrow/unknown

    if no scroll detector configured:
        return success

    ask scroll detector to flush any buffered arrow

    if detector returns buffered up arrow:
        handle arrow up
        propagate any error

    if detector returns buffered down arrow:
        handle arrow down
        propagate any error

    return success


function process_action(repl, input_action):
    validate inputs exist

    if action is arrow up or arrow down:
        attempt scroll detection
        if error occurs:
            return error
        if scroll detector handled it:
            return success (consumed)

    if action is not arrow or unknown:
        flush any pending arrow buffered by scroll detector
        if error occurs:
            return error

    route action by type:
        CHARACTER:
            if space and completion active:
                commit selection and dismiss completion

            if browsing history:
                stop history browsing

            reset viewport offset to 0
            insert character into input buffer
            if completion active:
                update completion based on new character

        INSERT_NEWLINE:
            reset viewport offset to 0
            insert newline into input buffer

        NEWLINE:
            process as line submission (evaluate input)

        BACKSPACE:
            reset viewport offset to 0
            delete character before cursor
            if completion active:
                update completion based on deletion

        DELETE:
            reset viewport offset to 0
            delete character at cursor

        ARROW_LEFT:
            dismiss completion
            reset viewport offset to 0
            move cursor left in buffer

        ARROW_RIGHT:
            dismiss completion
            reset viewport offset to 0
            move cursor right in buffer

        ARROW_UP:
            navigate history backward

        ARROW_DOWN:
            navigate history forward

        PAGE_UP:
            scroll scrollback up one page

        PAGE_DOWN:
            scroll scrollback down one page

        SCROLL_UP:
            scroll scrollback up

        SCROLL_DOWN:
            scroll scrollback down

        CTRL_A:
            reset viewport offset to 0
            move cursor to start of current line

        CTRL_E:
            reset viewport offset to 0
            move cursor to end of current line

        CTRL_K:
            reset viewport offset to 0
            delete everything from cursor to end of line

        CTRL_N:
            navigate history forward

        CTRL_P:
            navigate history backward

        CTRL_U:
            reset viewport offset to 0
            delete entire line

        CTRL_W:
            reset viewport offset to 0
            delete word before cursor

        CTRL_C:
            set quit flag (exit REPL)

        TAB:
            trigger completion or advance through completions

        ESCAPE:
            if completion active and original input stored:
                restore original input text before completion started
            dismiss completion

        NAV_PREV_SIBLING, NAV_NEXT_SIBLING, NAV_PARENT, NAV_CHILD:
            navigate tree structure

        UNKNOWN:
            do nothing

    return success
```
