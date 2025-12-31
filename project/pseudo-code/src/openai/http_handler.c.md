## Overview

This module provides low-level HTTP client functionality for OpenAI's API. It handles libcurl operations for making POST requests, manages server-sent events (SSE) streaming responses, and extracts response metadata including content, finish reasons, and tool calls.

## Code

```
function extract_finish_reason(sse_event) -> string or null:
    validate event has "data: " prefix

    extract JSON payload after prefix

    if payload is "[DONE]" marker:
        return null

    parse JSON from payload
    if parse fails:
        return null

    validate root is JSON object

    navigate to choices[0].finish_reason in JSON
    if not found or invalid:
        return null

    extract and return the finish_reason string


function on_http_data_received(data) -> size written:
    feed data to SSE event parser

    while more complete events available:
        get next SSE event from parser
        if no event available:
            break

        try to parse content from this event
        if parsing fails:
            skip to next event

        if content was extracted:
            if user provided streaming callback:
                invoke callback with content
                if callback reports error:
                    mark error state and stop processing

            accumulate content into complete response buffer

        else (no content means possible tool call):
            try to parse tool calls from event
            if tool call found:
                if first tool call chunk:
                    store tool call for later
                else (additional chunks):
                    append arguments to existing tool call

        try to extract finish_reason from event
        if found and not already stored:
            save finish_reason

        free event

    return amount of data processed


function post_to_openai_api(url, api_key, request_body, stream_callback) -> response or error:
    initialize libcurl handle
    if initialization fails:
        return error

    allocate callback context:
        - setup SSE parser
        - save stream callback and context
        - initialize accumulators for response, finish_reason, tool_calls

    configure HTTP request:
        - set URL
        - set POST method
        - set request body
        - attach write callback to process server data

    build HTTP headers:
        - set Content-Type to application/json
        - build Authorization header with API key
        if Authorization header too long:
            cleanup and return error
        - attach all headers to request

    perform HTTP request

    cleanup libcurl handle and headers

    if HTTP request failed:
        return error

    if callback reported error during streaming:
        return error

    allocate response structure

    transfer accumulated content to response:
        if no content received:
            store empty string
        else:
            store accumulated content

    transfer finish_reason to response if available

    transfer tool_call to response if available

    return success with response
```
