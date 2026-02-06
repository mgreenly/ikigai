## Overview

Helper functions for send and wait command implementations. Provides utilities for formatting relative timestamps, parsing UUIDs from command arguments, and rendering wait results to scrollback.

## Code

```
function format_timestamp(seconds_elapsed, output_buffer):
    if seconds_elapsed < 60:
        format as "N sec ago"
    else if seconds_elapsed < 3600:
        format as "N min ago"
    else if seconds_elapsed < 86400:
        format as "N hour(s) ago" (plural if > 1)
    else:
        format as "N day(s) ago" (plural if > 1)

    write result to output buffer


function render_wait_result(context, scrollback, results, result_count):
    // Render fan-in wait results to scrollback (slash command path)
    validate inputs (context, scrollback, and results array exist)

    for each result in results:
        construct status line:
            - agent name or truncated UUID
            - status indicator (checkmark for received, X for dead, spinner for running)
            - status text

        append status line to scrollback

        if status is "received":
            append message preview to scrollback
            truncate body to 50 characters max, adding "..." if truncated

    return success


function parse_uuid(input_string, uuid_output_buffer):
    skip any leading whitespace in input

    read non-whitespace characters until next space or end of string

    if no UUID text found:
        return false (failure)

    if UUID is 256+ characters (too long):
        return false (failure)

    copy UUID text to output buffer
    null-terminate the buffer
    return true (success)
```
