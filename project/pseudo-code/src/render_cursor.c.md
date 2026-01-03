## Overview

Calculates the visual screen position (row and column) where the cursor should appear based on a byte offset into the text. This accounts for UTF-8 character display widths, line wrapping at terminal boundaries, ANSI escape sequences that don't consume screen space, and newline characters.

## Code

```
function calculate_cursor_screen_position(context, text, text_length, cursor_byte_offset, terminal_width):
    validate context is not null
    validate text is not null
    validate output position is not null

    initialize current_row = 0
    initialize current_column = 0
    initialize current_position = 0

    // scan through text byte by byte until reaching the cursor position
    while current_position < cursor_byte_offset:

        // handle newline characters - they reset column and advance to next row
        if text[current_position] is newline:
            current_row++
            current_column = 0
            current_position++
            continue

        // skip ANSI escape sequences - they render nothing on screen
        bytes_to_skip = skip ANSI escape sequences starting at current position
        if bytes_to_skip > 0:
            current_position += bytes_to_skip
            continue

        // decode the next UTF-8 character at current position
        character_codepoint = decode UTF-8 from current position
        bytes_consumed = count bytes in this UTF-8 character

        // if UTF-8 decoding fails, character is invalid
        if bytes_consumed <= 0:
            return error: "Invalid UTF-8 at byte offset X"

        // determine how many screen columns this character occupies
        // (handles wide characters like CJK and combining characters)
        character_width = get display width of character_codepoint

        // check if character would exceed terminal width
        if current_column + character_width > terminal_width:
            // wrap to next line
            current_row++
            current_column = 0

        // advance column by character width and move to next byte
        current_column += character_width
        current_position += bytes_consumed

    // if cursor lands exactly at terminal width boundary, wrap to next line
    if current_column == terminal_width:
        current_row++
        current_column = 0

    // store calculated position
    output_position.screen_row = current_row
    output_position.screen_col = current_column

    return success
```
