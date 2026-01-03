## Overview

This module implements a Server-Sent Events (SSE) parser for streaming HTTP responses. It accumulates incoming data in a dynamically-growing buffer and extracts complete events delimited by double newlines. It also provides functions to parse SSE event payloads, extracting either text content or tool call information from OpenAI API streaming responses.

## Code

```
function create_sse_parser():
    allocate a new parser structure
    initialize a 4096-byte buffer with null terminator
    return parser

function feed_data_to_parser(parser, incoming_data):
    if incoming_data is empty:
        return

    if buffer cannot hold incoming_data:
        double buffer capacity until it fits
        reallocate buffer with new capacity

    append incoming_data to end of buffer
    update buffer length
    ensure buffer is null-terminated

function get_complete_event(parser):
    search for double newline (\n\n) in buffer
    if not found:
        return null (incomplete event)

    extract event string up to the double newline
    remove event and delimiter from buffer
    shift remaining data to start of buffer
    update buffer length
    return event string

function parse_sse_event_for_content(event_string):
    validate event starts with "data: " prefix
    extract JSON payload after the prefix

    check if payload is [DONE] marker (end of stream):
        return success with no content

    parse JSON document
    validate root is a JSON object

    navigate to choices[0].delta.content:
        if any level is missing or wrong type:
            return success with no content

    extract content string from JSON
    return success with content string

function parse_sse_event_for_tool_calls(event_string):
    validate event starts with "data: " prefix
    extract JSON payload after the prefix

    check if payload is [DONE] marker (end of stream):
        return success with no tool calls

    parse JSON document
    validate root is a JSON object

    navigate to choices[0].delta.tool_calls[0]:
        if any level is missing or wrong type:
            return success with no tool calls

    extract id field (may be absent in subsequent streaming chunks)
    extract function.name field (may be absent in subsequent chunks)

    validate that id and name have consistent presence:
        if one is present and the other is absent:
            return success with no tool calls

    extract function.arguments string

    create tool_call structure with:
        - id (empty string if not present)
        - function name (empty string if not present)
        - arguments as JSON string

    return success with tool_call structure
```
