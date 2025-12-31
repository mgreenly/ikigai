## Overview

Manages display layout calculations for the input buffer by computing how many physical (display) lines a logical text buffer occupies when wrapped to a terminal width. Handles UTF-8 text with variable-width characters and ANSI escape sequences. Caches layout information and invalidates it when the buffer changes.

## Code

```
function calculate_display_width(text, length):
    if text is empty:
        return 0

    display_width = 0
    position = 0

    while position < length:
        // Skip ANSI escape sequences (color codes, formatting, etc.)
        skip = skip ANSI escape sequence at position
        if skip > 0:
            position += skip
            continue

        // Decode next UTF-8 character and get its codepoint
        codepoint, bytes_read = decode UTF-8 character at position

        // Handle invalid UTF-8 gracefully (defensive)
        if bytes_read <= 0:
            display_width += 1
            position += 1
            continue

        // Get the display width of this character (0, 1, or 2 columns)
        char_width = get display width of codepoint
        if char_width >= 0:
            display_width += char_width

        position += bytes_read

    return display_width


function ensure_layout(input_buffer, terminal_width):
    assert input_buffer exists

    // Optimization: if layout is clean and terminal width hasn't changed, no work needed
    if layout is clean AND cached terminal width equals current terminal width:
        return

    // Get the text content from the input buffer
    text = get text from input buffer

    // Handle empty buffer (Bug #10 fix)
    if text is empty:
        physical_lines = 0
        cache terminal width
        mark layout as clean
        return

    // Scan through the text and count physical lines needed
    physical_lines = 0
    line_start = 0

    for each position in text (including end):
        // Process complete logical lines (ended by newline or EOF)
        if found newline OR at end of text:
            logical_line = text from line_start to current position
            line_length = length of logical_line

            if line_length is 0:
                // Empty line (just a newline character)
                physical_lines += 1
            else:
                // Calculate how wide this line appears when displayed
                display_width = calculate_display_width(logical_line)

                if display_width is 0:
                    // Rare case: line contains only zero-width characters
                    physical_lines += 1
                else if terminal_width > 0:
                    // Calculate how many rows this logical line needs when wrapped
                    // Use ceiling division: (width + terminal_width - 1) / terminal_width
                    physical_lines += ceiling(display_width / terminal_width)
                else:
                    // Defensive: terminal width should always be positive
                    physical_lines += 1

            // Move to start of next logical line
            line_start = next position after newline

    // Update the cached layout information
    cache physical_lines count
    cache terminal width
    mark layout as clean


function invalidate_layout(input_buffer):
    assert input_buffer exists

    // Mark layout cache as dirty so it will be recalculated next time
    set layout_dirty flag


function get_physical_lines(input_buffer):
    assert input_buffer exists

    // Return the number of display rows the input buffer currently occupies
    return cached physical_lines count
```
