## Overview

This file provides helper functions for calculating byte offsets within scrollback lines based on terminal display positions. It handles the complex task of mapping from row-column display coordinates to byte offsets in UTF-8 encoded text, accounting for text wrapping at terminal width boundaries and embedded newline segments.

## Code

```
function calc_start_byte_for_row(scrollback, line_index, terminal_width, start_row_offset):
    // Map a display row position to the byte offset where that row begins

    if no rows to skip:
        return byte 0 (start of line)

    // Get the line text and its layout information
    line_text = retrieve line content
    line_length = length of line in bytes
    segment_count = number of segments (split by newlines) in this line
    segment_widths = display widths of each segment

    // Find which segment contains the starting row
    rows_remaining = start_row_offset
    current_segment_index = 0
    partial_rows_in_segment = 0

    while there are more segments and rows to skip:
        rows_in_current_segment = segment width divided by terminal width (rounded up)

        if we can skip this entire segment:
            rows_remaining -= rows_in_segment
            move to next segment
        else:
            partial_rows_in_segment = rows_remaining (partial segment)
            rows_remaining = 0

    // Calculate total display columns to skip
    columns_from_previous_segments = sum of all segment widths before current segment
    total_columns_to_skip = columns_from_previous_segments + (partial_rows_in_segment * terminal_width)

    // Convert display columns to byte offset
    start_byte = convert display columns to byte offset

    if an error occurs during conversion:
        return 0 (fallback to line start)

    // If we skipped complete segments, skip past their newline characters
    if we skipped at least one segment:
        newlines_to_skip = current_segment_index
        newlines_counted = 0

        for each byte in line:
            if byte is newline:
                newlines_counted += 1
                if we've counted enough newlines:
                    return position after this newline

    return start_byte


function calc_end_byte_for_row(scrollback, line_index, terminal_width, end_row_offset, is_line_end_out):
    // Map a display row position to the byte offset where that row ends

    line_text = retrieve line content
    line_length = length of line in bytes
    total_physical_rows = how many rows this line occupies on terminal

    // Check if we're rendering to the very end of the line
    if end_row_offset reaches or exceeds the last row:
        mark that we're at line end
        return entire line length

    mark that we're not at line end

    // Find which segment contains the ending row
    segment_count = number of segments in this line
    segment_widths = display widths of each segment

    rows_to_include = end_row_offset + 1
    current_segment_index = 0
    partial_rows_in_segment = 0

    while there are more segments and rows to include:
        rows_in_current_segment = segment width divided by terminal width (rounded up)

        if we can include this entire segment:
            rows_to_include -= rows_in_segment
            move to next segment
        else:
            partial_rows_in_segment = rows_to_include (partial segment)
            rows_to_include = 0

    // Calculate total display columns to include
    columns_from_previous_segments = sum of all segment widths before current segment
    total_columns_to_include = columns_from_previous_segments + (partial_rows_in_segment * terminal_width)

    // Convert display columns to byte offset
    end_byte = convert display columns to byte offset

    if an error occurs during conversion:
        return entire line length (fallback to line end)

    // If we included complete segments, include their newline characters
    if we included at least one segment:
        newlines_to_include = current_segment_index
        newlines_counted = 0

        for each byte in line:
            if byte is newline:
                newlines_counted += 1
                if we've counted enough newlines:
                    return position after this newline

    return end_byte


function calc_byte_range_for_rows(scrollback, line_index, terminal_width,
                                   start_row_offset, row_count,
                                   start_byte_out, end_byte_out, is_line_end_out):
    // Calculate both start and end byte offsets for a range of display rows

    start_byte = calculate start byte for starting row offset

    ending_row_offset = starting_row_offset + number_of_rows - 1
    end_byte, is_at_line_end = calculate end byte for ending row offset

    // Return results via output parameters
    set start_byte_out = start_byte
    set end_byte_out = end_byte
    set is_line_end_out = is_at_line_end
```
