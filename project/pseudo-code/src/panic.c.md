## Overview

This file provides emergency error handling for fatal conditions. When a panic occurs, it safely restores the terminal state (showing cursor, resetting text attributes, exiting alternate screen mode) and logs the error before terminating the process. All functions are designed to be async-signal-safe to handle crashes in signal handlers without triggering undefined behavior.

## Code

```
function safe_strlen(string):
    count = 0
    if string is not null:
        while string[count] is not null:
            increment count
    return count

function int_to_str(number, output_buffer, buffer_size):
    if buffer_size is zero:
        return 0

    index = 0
    is_negative = (number < 0)
    if is_negative:
        negate number

    // Extract digits in reverse into temporary buffer
    temporary_buffer = empty
    digit_count = 0
    do:
        temporary_buffer[digit_count] = '0' + (number mod 10)
        increment digit_count
        number = number div 10
    while (number > 0 and digit_count < 15)

    // Write negative sign if needed
    if is_negative and index < (buffer_size - 1):
        output_buffer[index] = '-'
        increment index

    // Reverse digits into output buffer
    while (digit_count > 0 and index < (buffer_size - 1)):
        decrement digit_count
        output_buffer[index] = temporary_buffer[digit_count]
        increment index

    output_buffer[index] = null terminator
    return index

function write_ignore(file_descriptor, data, size):
    // Write data to file descriptor, ignoring the result
    // This is necessary to suppress compiler warnings in panic handlers
    result = write(file_descriptor, data, size)

function ik_panic_impl(error_message, source_file, line_number):
    // Step 1: Restore terminal if we have a terminal context
    if terminal context exists:
        write escape sequence to show cursor, reset text attributes, exit alternate screen
        restore original terminal settings using tcsetattr

    // Step 2: Attempt to write panic event to logger
    read volatile logger reference
    if logger is available:
        get logger file descriptor
        if descriptor is valid:
            format JSON event: {"level":"fatal","event":"panic","message":"...","file":"...","line":N}
            write formatted message to logger

    // Step 3: Write human-readable error to stderr
    convert line number to string
    write "FATAL: " to stderr
    if error_message exists:
        write error_message to stderr
    write newline and location info to stderr
    write source file name to stderr
    write colon and line number to stderr
    write final newline to stderr

    // Step 4: Terminate process
    abort()

function ik_talloc_abort_handler(reason):
    // Talloc calls this when it detects internal corruption or double free
    if reason is provided:
        call ik_panic_impl(reason, "<talloc>", 0)
    else:
        call ik_panic_impl("talloc abort", "<talloc>", 0)
```
