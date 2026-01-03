## Overview

A command history manager that maintains a circular buffer of commands with deduplication and browsing capabilities. Supports navigating backward/forward through history while preserving a "pending" input that users can modify before re-executing. Automatically moves duplicate entries to the end and removes oldest entries when capacity is reached.

## Code

```
function create_history(context, capacity):
    validate context and capacity are valid

    allocate a history structure with zero initialization
    set history capacity
    initialize entry count to 0
    initialize browse position to 0 (not browsing)
    clear pending input

    allocate array of entries

    return history


function add_to_history(history, entry):
    validate history and entry exist

    if entry is empty:
        return success

    if most recent entry matches this entry:
        reset browse position to end
        clear any pending input
        return success

    search for existing matching entry in history

    if found:
        free the duplicate entry
        shift all entries after it down by one position
        decrement count

    if history is at capacity:
        free oldest entry
        shift all entries down by one position
        decrement count

    add new entry to the end
    increment count

    reset browse position to end
    clear any pending input

    return success


function start_browsing(history, pending_input):
    validate history and pending input exist

    if history is empty:
        save pending input
        keep browse position at 0 (still not browsing)
        return success

    save the current pending input

    set browse position to last entry (count - 1)

    return success


function go_to_previous(history):
    validate history exists

    if not currently browsing or at the beginning:
        return null

    move browse position backward one entry

    return entry at current position


function go_to_next(history):
    validate history exists

    if browse position is past all entries:
        return null

    if at the pending position:
        move forward past pending
        return null

    move browse position forward one position

    if still within history entries:
        return entry at current position

    return the pending input


function stop_browsing(history):
    validate history exists

    move browse position to "not browsing" state (past the end)

    free and clear the pending input


function get_current_entry(history):
    validate history exists

    if not browsing (at or past the end):
        return pending input (may be null)

    return entry at current browse position


function is_currently_browsing(history):
    validate history exists

    return true if browse position is within history entries, false otherwise
```
