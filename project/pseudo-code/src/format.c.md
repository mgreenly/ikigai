## Overview

Format buffer management for building formatted strings dynamically. Provides a growable string buffer with functions to append formatted text, plain strings, and indentation. Includes specialized formatters for tool calls and tool results that parse and pretty-print JSON arguments and outputs.

## Code

```
// Buffer creation and basic operations
function create_format_buffer():
    allocate buffer structure
    allocate underlying byte array (32-byte increment)
    return buffer

function append_formatted(buffer, format_string, ...):
    validate buffer and format string

    first pass: determine how many bytes are needed for the formatted output
    allocate temporary buffer to hold the formatted result

    second pass: format the arguments into the temporary buffer
    validate formatting succeeded and no truncation occurred

    byte-by-byte: append each character from temporary buffer to the byte array
    free temporary buffer
    return success

function append_string(buffer, string):
    validate buffer and string
    skip if string is empty

    byte-by-byte: append each character to the byte array
    return success

function append_indent(buffer, indent_level):
    validate buffer and indent level is non-negative
    skip if indent level is zero

    append indent_level spaces to the byte array
    return success

function get_string(buffer):
    validate buffer

    if buffer doesn't end with null terminator:
        append null terminator to byte array

    return pointer to the data buffer

function get_length(buffer):
    validate buffer

    get size of byte array
    if last byte is null terminator:
        return size minus 1 (exclude the terminator)
    else:
        return size as-is

// Tool call formatting with JSON parsing
function format_tool_call(tool_call):
    create new format buffer

    append "→ " followed by tool name

    if tool has no arguments:
        return formatted string

    try to parse arguments as JSON

    if JSON parsing fails:
        append raw arguments string and return

    if JSON is not an object:
        append raw arguments string and return

    if object is empty:
        return formatted string (no arguments to display)

    append ": " separator

    for each key-value pair in the JSON object:
        if not the first pair:
            append ", " separator

        append the key
        append "="

        format value based on its type:
            if string:
                append value wrapped in double quotes
            if integer:
                append the integer value
            if float:
                append the floating-point value
            if boolean:
                append "true" or "false"
            if null:
                append "null"
            if array or object:
                serialize to JSON and append

    return formatted string

// Tool result formatting with JSON parsing and truncation
function format_tool_result(tool_name, result_json):
    create new format buffer

    append "← " followed by tool name and ": " separator

    if result_json is null:
        append "(no output)"
        return formatted string

    try to parse result_json as JSON

    if JSON parsing fails:
        truncate and append raw JSON string
        return formatted string

    extract content based on JSON type:
        if string value:
            use the string directly
            if empty: append "(no output)" and return

        if array value:
            for each element in the array:
                if string element: append string
                else: serialize to JSON and append
                elements separated by ", "

        if object or other type:
            serialize entire JSON to string

    truncate content to 3 lines OR 400 characters (whichever comes first)
    append truncated content

    return formatted string

// Helper: truncate content intelligently
function truncate_and_append(buffer, content, length):
    validate buffer and content

    if content is empty:
        append "(no output)"
        return

    scan through content:
        count newlines
        count characters

        if encounter more than 3 newlines:
            stop at the line boundary
            mark as needing truncation

        if reach 400 characters:
            stop at current position
            mark as needing truncation

    if truncation needed:
        append content up to truncation point with "..." at end
    else:
        append full content

// Helper: extract array elements from JSON
function extract_array_content(json_array):
    create new format buffer

    for each element in the array:
        if not the first element:
            append ", "

        if element is string:
            append string value
        else:
            serialize element to JSON and append

    return the formatted content

// Wrappers for vendor JSON library functions
// (These are not-inline versions for testability and coverage)
function yyjson_obj_iter_init_wrapper(json_object, iterator):
    initialize iterator over JSON object fields

function yyjson_obj_iter_next_wrapper(iterator):
    get next key from object iterator

function yyjson_obj_iter_get_val_wrapper(key):
    get value associated with the current key

function yyjson_val_write_wrapper(json_value):
    serialize JSON value to string
```
