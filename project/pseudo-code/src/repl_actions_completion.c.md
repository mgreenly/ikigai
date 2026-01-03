## Overview

This module handles command completion functionality in the REPL. It manages the lifecycle of completion contexts: triggering completion when the user types "/" or presses Tab, filtering matches as the user types more characters, cycling through matches, committing selections with Space, and reverting to original input with Escape.

## Code

```
function dismiss_completion(repl):
    if completion context exists:
        free and clear completion

function update_completion_after_typing_character(repl):
    get current input text

    if input starts with '/':
        preserve original_input if already set (for Escape revert)

        create prefix string from current input

        create new completion with updated prefix
        free old completion
        assign new completion

        if new completion exists and we have original_input:
            restore original_input to new completion
        else if original_input exists:
            free it
    else:
        dismiss completion (input doesn't start with '/')

function build_completion_text(repl, completion, suffix):
    get currently selected completion match
    if no selection:
        return failure

    get original_input (what user typed before Tab)
    if no original_input, use prefix instead

    create current_input string

    find space in current_input to detect completion type

    if space exists (argument completion):
        text = command_part + selected_match + suffix
    else (command completion):
        text = "/" + selected_match + suffix

    free current_input
    return constructed text

function update_input_buffer_with_selection(repl):
    validate completion exists

    build text for selected completion (no suffix)
    if build fails:
        return success (do nothing)

    replace input buffer with selected completion text
    if replacement fails:
        return error

    move cursor to end of line
    if cursor move fails:
        return error

    return success

function handle_space_key_with_completion(repl):
    if no completion active:
        return success (do nothing)

    build text for selected completion with space suffix
    if build fails:
        dismiss completion
        return success

    dismiss completion

    replace input buffer with selected completion + space
    if replacement fails:
        return error

    move cursor to end of line
    if cursor move fails:
        return error

    return success

function handle_tab_key(repl):
    if completion already active:
        if original_input not yet recorded:
            save current input as original_input

        advance to next completion match

        update input buffer with new selection
        if update fails:
            dismiss completion
            return error

        dismiss completion
        return success

    get current input text

    if input is empty or doesn't start with '/':
        return success (do nothing)

    create prefix string from input
    save prefix as original_input (for Escape revert)

    find space in prefix to detect completion type

    if space exists (argument completion):
        create argument completion
    else (command completion):
        create command completion

    free prefix

    if completion created successfully:
        store original_input in completion

        update input buffer with first selection
        if update fails:
            dismiss completion
            return error

        dismiss completion
    else:
        free original_input

    return success
```
