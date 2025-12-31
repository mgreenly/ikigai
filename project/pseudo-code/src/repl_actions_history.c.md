## Overview

This file implements history navigation actions for the REPL. It handles three main operations: arrow up/down navigation (which move the cursor within multi-line input or navigate completions), and Ctrl+P/N navigation (which browse through command history). The file manages the transition between editing current input and browsing historical entries, preserving the pending input when starting history browsing.

## Code

```
helper function load_history_entry(repl, entry):
    replace the input buffer contents with the history entry text
    reset cursor to position 0
    return success or failure

function handle_arrow_up_action(repl):
    // Arrow keys primarily handle cursor movement and viewport scrolling

    if the viewport is scrolled (showing scrollback):
        scroll the viewport up instead
        return success

    if completion menu is active:
        navigate to previous completion candidate
        return success

    check cursor position in the input buffer
    if cursor is not at the start of a line:
        move cursor up within the input buffer
        return success

    validate history is available

    get the current text from the input buffer

    if not currently browsing history:
        save the current input text as pending
        start browsing history with this pending text
        get the first history entry matching the pending text
    else:
        navigate to the previous history entry

    if no entry found:
        return success (stay at current position)

    load the history entry into the input buffer
    return success

function handle_arrow_down_action(repl):
    // Arrow keys primarily handle cursor movement and viewport scrolling

    if the viewport is scrolled (showing scrollback):
        scroll the viewport down instead
        return success

    if completion menu is active:
        navigate to next completion candidate
        return success

    check cursor position in the input buffer
    if cursor is not at the start of a line:
        move cursor down within the input buffer
        return success

    if currently browsing history:
        navigate to the next history entry
        if an entry exists:
            load the history entry into the input buffer
            return success

    move cursor down within the input buffer
    return success

function handle_history_prev_action(repl):
    // Ctrl+P: dedicated history navigation key

    validate history is available

    get the current text from the input buffer

    if not currently browsing history:
        save the current input text as pending
        start browsing history with this pending text
        get the first history entry matching the pending text
    else:
        navigate to the previous history entry

    if no entry found:
        return success (stay at current position)

    load the history entry into the input buffer
    return success

function handle_history_next_action(repl):
    // Ctrl+N: dedicated history navigation key

    validate history is available and browsing is active

    navigate to the next history entry
    if no entry found:
        return success (stay at current position)

    load the history entry into the input buffer
    return success
```
