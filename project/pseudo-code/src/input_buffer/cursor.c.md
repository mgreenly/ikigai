## Overview

This file implements cursor position tracking for text input with support for both byte offsets (for UTF-8 operations) and grapheme cluster offsets (for visual/user-facing position). The cursor maintains synchronized position state in both metrics, handling multi-byte UTF-8 characters and complex grapheme clusters (combining diacritics, emoji variations, etc.).

## Code

```
function create_cursor(parent context):
    validate parent context is provided

    allocate cursor structure from parent context
    if allocation fails, panic with out of memory error

    initialize byte_offset to 0
    initialize grapheme_offset to 0

    return cursor


function set_cursor_position(cursor, text, text length, byte offset):
    validate cursor exists
    validate text exists
    validate byte offset is within text bounds

    set cursor's byte_offset to the requested position

    count grapheme clusters from start of text to byte offset:
        grapheme_count = 0
        current_byte_position = 0
        previous_codepoint = none

        while current_byte_position < byte offset:
            decode UTF-8 codepoint at current position (read bytes_read)
            if decoding fails, panic (text validity guaranteed)

            if this is the first codepoint or grapheme boundary detected:
                increment grapheme_count

            update previous_codepoint
            advance current_byte_position by bytes_read

    set cursor's grapheme_offset to grapheme_count


function move_cursor_left(cursor, text, text length):
    validate cursor exists
    validate text exists

    if cursor is at start of text:
        return (no-op)

    find the previous grapheme boundary before current cursor position:
        last_boundary_byte = 0
        grapheme_count = 0
        current_byte_position = 0
        previous_codepoint = none

        while current_byte_position < text length:
            decode UTF-8 codepoint at current position (read bytes_read)
            if decoding fails, panic (text validity guaranteed)

            if this is first codepoint or grapheme boundary detected:
                if this boundary is before cursor's byte_offset:
                    save this boundary as last_boundary_byte
                    increment grapheme_count
                else:
                    stop searching (reached/passed cursor position)

            update previous_codepoint
            advance current_byte_position by bytes_read

    move cursor to last_boundary_byte
    set grapheme_offset to (grapheme_count - 1) or 0 if count is 0


function move_cursor_right(cursor, text, text length):
    validate cursor exists
    validate text exists

    if cursor is at end of text:
        return (no-op)

    find next grapheme boundary after current cursor position:
        current_byte_position = cursor's current byte_offset
        previous_codepoint = none
        found_next_boundary = false
        next_boundary_byte = text_length

        while current_byte_position < text length:
            decode UTF-8 codepoint at current position (read bytes_read)
            if decoding fails, panic (text validity guaranteed)

            advance current_byte_position by bytes_read

            if previous_codepoint exists and grapheme boundary detected:
                next_boundary_byte = current_byte_position - bytes_read
                found_next_boundary = true
                stop searching

            update previous_codepoint

        if grapheme boundary not found:
            next_boundary_byte = current_byte_position (text end)

    move cursor to next_boundary_byte
    increment grapheme_offset by 1


function get_cursor_position(cursor):
    validate cursor exists
    validate output parameters provided

    retrieve cursor's byte_offset
    retrieve cursor's grapheme_offset

    return both offsets to caller
```
