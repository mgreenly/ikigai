## Overview

This module manages conversation marks - labeled or unlabeled checkpoints within the conversation history. Marks record the current message count and timestamp, enabling users to create snapshots of conversation state and rewind to previous points. The module provides functions to create marks, find marks by label, and rewind the entire conversation (including message history and associated marks) to a specific mark's position.

## Code

```
function get_iso8601_timestamp(memory_context):
    get current system time

    convert time to UTC format
    if conversion fails:
        return error "gmtime failed"

    allocate 32-byte string buffer
    if allocation fails:
        panic with "Out of memory"

    format timestamp as "YYYY-MM-DDTHH:MM:SSZ"
    if formatting fails:
        free buffer
        return error "strftime failed"

    return success with timestamp string


function create_mark(repl, label):
    validate repl exists
    // label can be null for unlabeled marks

    allocate new mark structure
    if allocation fails:
        panic with "Out of memory"

    record current conversation message count in mark

    if label was provided:
        copy label string to mark
        if copy fails:
            panic with "Out of memory"
    else:
        leave mark label as null

    generate ISO 8601 timestamp
    if timestamp generation fails:
        free mark
        return error

    attach timestamp to mark

    grow the marks array by one element
    if reallocation fails:
        panic with "Out of memory"

    add mark to end of marks array
    increment mark count

    // Change ownership: mark now belongs to the marks array

    // Create event for rendering to scrollback
    format data as JSON: {"label": "..."} or {}
    if formatting fails:
        panic with "Out of memory"

    render "mark" event to scrollback
    free temporary JSON string
    if rendering fails:
        return error

    return success


function find_mark(repl, label, output):
    validate repl and output exist
    // label can be null to find most recent mark

    if no marks exist:
        return error "No marks found"

    if label is null:
        return the most recent mark (last in array)

    search marks array from most recent to oldest:
        for each mark:
            if mark has a label and label matches:
                return found mark

    // Mark with label not found
    return error "Mark not found: {label}"


function rewind_to_mark(repl, target_mark):
    validate repl and target_mark exist

    // Truncate conversation history at mark position
    free all messages after the mark's message index
    set conversation message count to mark's message index

    // Clean up marks after target
    find the index of target_mark in marks array
    assert mark was found

    free all marks after target_mark
    set mark count to target_mark's index + 1 (keeps target mark)

    // Rebuild scrollback display from scratch
    clear all scrollback content

    // Re-render system message if configured
    if system message exists:
        render "system" event to scrollback
        if rendering fails:
            return error

    // Re-render all remaining conversation messages
    for each message in conversation:
        render message event to scrollback (use message kind and content)
        if rendering fails:
            return error

    // Re-add visual indicators for remaining marks
    for each mark in remaining marks:
        format mark data as JSON: {"label": "..."} or {}
        if formatting fails:
            panic with "Out of memory"

        render "mark" event to scrollback
        free temporary JSON string
        if rendering fails:
            return error

    // Note: Rewind operation itself doesn't render anything new to scrollback
    // The visual result is simply: system message + conversation + marks

    return success


function rewind_to(repl, label):
    validate repl exists
    // label can be null to rewind to most recent mark

    find mark by label
    if mark not found:
        return error

    rewind conversation to the found mark
    return result
```
