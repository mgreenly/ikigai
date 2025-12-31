## Overview

This file provides utility functions for analyzing scrollback text. It handles the complexities of text display by accounting for ANSI escape sequences (which don't affect display width), UTF-8 character encoding, and character width calculations. The module helps measure how much visible space text will occupy on a terminal and count line breaks for layout calculations.

## Code

```
function calculate_display_width(text, length):
    display_width = 0
    position = 0

    while position < length:
        // Skip over ANSI escape sequences that don't display
        skip_bytes = skip_ansi_escape_sequences(text, length, position)
        if skip_bytes > 0:
            position += skip_bytes
            continue

        // Decode the next UTF-8 character at current position
        character = decode_utf8_character(text, position, length)

        // Handle invalid UTF-8
        if character is invalid:
            display_width += 1
            position += 1
            continue

        // Newlines don't contribute to display width
        if character is newline:
            position += character_byte_length
            continue

        // Add the character's display width (most are 1, some are 2 for wide chars)
        character_width = get_character_display_width(character)
        if character_width > 0:
            display_width += character_width

        position += character_byte_length

    return display_width


function count_newlines(text, length):
    newline_count = 0
    position = 0

    while position < length:
        // Skip over ANSI escape sequences
        skip_bytes = skip_ansi_escape_sequences(text, length, position)
        if skip_bytes > 0:
            position += skip_bytes
            continue

        // Decode the next UTF-8 character
        character = decode_utf8_character(text, position, length)

        // Invalid UTF-8 doesn't affect count
        if character is invalid:
            position += 1
            continue

        // Count newlines
        if character is newline:
            newline_count += 1

        position += character_byte_length

    return newline_count


function trim_trailing_whitespace(parent_context, text, length):
    validate parent_context is not null

    if text is null or length is 0:
        return empty string allocated in parent context

    // Find the last non-whitespace character
    // (skip spaces, tabs, newlines, carriage returns at the end)
    end_position = length
    while end_position > 0:
        character = text[end_position - 1]
        if character is not space, tab, newline, or carriage return:
            break
        end_position -= 1

    // Return a substring from beginning to end_position, allocated in parent context
    return allocate_string_copy(parent_context, text, end_position)
```
