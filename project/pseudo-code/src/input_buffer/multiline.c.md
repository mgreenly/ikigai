## Overview

Multi-line input buffer navigation and editing. Provides cursor movement across lines (up/down), horizontal movement to line boundaries (start/end), and operations to delete text to line end or kill entire lines. All cursor movements preserve a target column for consistent vertical navigation and properly handle UTF-8 encoded text by counting grapheme clusters.

## Code

```
private function find_line_start(text, cursor_pos):
    if cursor is at position 0:
        return 0

    scan backward from cursor position until finding newline or reaching start
    return position of start (either after the previous newline, or 0)

private function find_line_end(text, text_len, cursor_pos):
    scan forward from cursor position until finding newline or reaching end of text
    return position of end (either the newline character, or text_len)

private function count_graphemes(text, len):
    // text must be valid UTF-8
    if length is 0:
        return 0

    count = 0
    iterate through text by examining UTF-8 byte patterns:
        if first byte indicates ASCII (1 byte):
            advance by 1
        else if first byte indicates 2-byte UTF-8:
            advance by 2
        else if first byte indicates 3-byte UTF-8:
            advance by 3
        else if first byte indicates 4-byte UTF-8:
            advance by 4
        else:
            panic - invalid UTF-8 encountered
        increment count

    return count of grapheme clusters

private function grapheme_to_byte_offset(text, len, target_grapheme):
    // text must be valid UTF-8
    if length is 0 or target is 0:
        return 0

    count = 0
    byte_pos = 0

    iterate through text by examining UTF-8 byte patterns:
        if we have reached the target grapheme count:
            break

        determine byte length of current character (1, 2, 3, or 4 bytes)
        advance byte position and increment count

    return byte offset where the target grapheme starts

public function cursor_up(input_buffer):
    validate input_buffer and cursor exist

    retrieve text and length
    if text is empty:
        return success (no-op)

    get current cursor position
    find start of current line

    if already on first line:
        return success (no-op)

    calculate current column (in graphemes, from line start to cursor)
    if target column not yet set during vertical movement:
        save current column as target

    find previous line boundaries:
        previous line starts after the newline before current line
        previous line ends at the newline before current line

    calculate previous line length (in graphemes)

    determine target column:
        use saved target column, or current column if just starting
        clamp to previous line's length

    move cursor to target column position on previous line
    update cursor object with new position
    return success

public function cursor_down(input_buffer):
    validate input_buffer and cursor exist

    retrieve text and length
    if text is empty:
        return success (no-op)

    get current cursor position
    find current line start and end

    if already on last line (no newline after):
        return success (no-op)

    calculate current column (in graphemes, from line start to cursor)
    if target column not yet set during vertical movement:
        save current column as target

    find next line boundaries:
        next line starts after the newline at current line end
        find where next line ends

    calculate next line length (in graphemes)

    determine target column:
        use saved target column, or current column if just starting
        clamp to next line's length

    move cursor to target column position on next line
    update cursor object with new position
    return success

public function cursor_to_line_start(input_buffer):
    validate input_buffer and cursor exist

    retrieve text and length
    if text is empty:
        return success (no-op)

    get current cursor position
    find start of current line

    if already at line start:
        return success (no-op)

    move cursor to line start
    update cursor object with new position
    clear target column (horizontal movement resets it)
    return success

public function cursor_to_line_end(input_buffer):
    validate input_buffer and cursor exist

    retrieve text and length
    if text is empty:
        return success (no-op)

    get current cursor position
    find end of current line (position of newline or text end)

    if already at line end:
        return success (no-op)

    move cursor to line end
    update cursor object with new position
    clear target column (horizontal movement resets it)
    return success

public function kill_to_line_end(input_buffer):
    validate input_buffer and cursor exist

    retrieve text and length
    if text is empty:
        return success (no-op)

    get current cursor position
    find end of current line

    if cursor is already at or past line end:
        return success (no-op)

    delete all bytes from cursor to line end

    retrieve updated text (no reallocation)
    update cursor object to reflect text change
    clear target column (text modification resets it)
    invalidate layout cache
    return success

public function kill_line(input_buffer):
    validate input_buffer and cursor exist

    retrieve text and length
    if text is empty:
        return success (no-op)

    get current cursor position
    find current line boundaries (start and end positions)

    determine deletion range:
        start from line start
        if line has a newline at the end, include it in deletion
        otherwise just delete up to line end

    delete entire line from determined range

    retrieve updated text (no reallocation)
    position cursor at where the deleted line was:
        clamp to not exceed new text length
    update cursor object
    clear target column
    invalidate layout cache
    return success
```
