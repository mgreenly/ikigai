## Overview

This file implements a debug output pipe system that captures debug output from child processes. It provides both individual pipe management (create, read, with automatic prefix handling) and a manager for coordinating multiple debug pipes, including integration with event-driven I/O polling and scrollback display.

## Code

```
function create_debug_pipe(parent_context, prefix_string):
    create a Unix pipe with two file descriptors (read and write)

    validate pipe creation succeeded, return error if it failed

    set read end to non-blocking mode (don't block on read attempts)

    convert write end file descriptor to a FILE stream for convenient writing

    allocate pipe structure with parent context

    if prefix string provided, copy it into the pipe structure

    allocate initial line buffer (1KB capacity) for accumulating incomplete lines

    register destructor to clean up resources when pipe is freed

    return pipe structure


function cleanup_pipe_resources(pipe):
    if write end FILE stream exists:
        close it (implicitly closes the underlying file descriptor)

    if read file descriptor is valid:
        close it


function read_debug_pipe(pipe):
    read up to 4KB of data from pipe's read end

    if read failed:
        if error is "would block" (no data available):
            return empty (normal case for non-blocking)
        return error

    if no data available:
        return empty

    allocate array for output lines (start with capacity for 16 lines)

    for each byte in the read data:
        if byte is newline:
            construct complete line by combining:
                - prefix (if specified)
                - space after prefix
                - accumulated buffer contents

            allocate new line string with prefix + content

            add line to output array (grow array if at capacity)

            reset buffer for next line
        else:
            add byte to accumulation buffer

            if buffer at capacity:
                double the buffer size and reallocate

    return array of complete lines


function create_debug_manager(parent_context):
    allocate manager structure with parent context

    initialize with:
        - capacity = 4 pipes
        - count = 0 pipes

    allocate pipes array with initial capacity

    return manager


function add_pipe_to_manager(manager, prefix_string):
    if pipes array at capacity:
        double the capacity and reallocate array

    create new debug pipe with the specified prefix

    if pipe creation failed:
        return error

    add pipe to manager's pipes array

    increment pipe count

    return pipe


function register_pipes_with_event_polling(manager, file_descriptor_set, max_fd_tracker):
    for each pipe in manager:
        add pipe's read file descriptor to the event set

        update max file descriptor tracker if this fd is larger


function handle_ready_pipes(manager, file_descriptor_set, scrollback_display, debug_enabled):
    for each pipe in manager:
        if pipe's read fd is not ready for reading:
            skip this pipe

        read debug output from pipe

        if read failed:
            return error

        if debug is enabled and lines were read:
            for each line:
                append line to scrollback display
                append blank line for visual spacing

        free the lines array

    return success
```
