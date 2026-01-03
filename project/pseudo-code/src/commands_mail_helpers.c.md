## Overview

Helper functions for mail command implementations. Provides utilities for formatting relative timestamps, rendering mail message lists to a scrollback display, and parsing command-line arguments (UUID and numeric indices).

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


function render_mail_list(context, scrollback, messages, message_count):
    validate inputs (context, scrollback, and messages array exist)

    get current time

    for each message in messages:
        calculate seconds elapsed since message was received

        format elapsed time as relative timestamp (using format_timestamp)

        construct message summary line:
            - message number (1-indexed)
            - unread indicator (asterisk if unread, space if read)
            - sender UUID truncated to 22 characters
            - relative timestamp

        append summary line to scrollback
        if append fails, propagate error

        construct preview line with message body text
        truncate body to 50 characters max, adding "..." if truncated
        wrap preview in quotes

        append preview line to scrollback
        if append fails, propagate error

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


function parse_index(input_string, index_output):
    if input_string is empty or null:
        return false (failure)

    parse string as base-10 integer

    if parsing failed or value is less than 1:
        return false (failure)

    store parsed index to output
    return true (success)
```
