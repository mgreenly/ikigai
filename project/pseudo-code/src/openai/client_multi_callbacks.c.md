## Overview

This file provides internal HTTP callback handlers for OpenAI's multi-client implementation. It processes Server-Sent Events (SSE) from OpenAI's streaming API responses, extracting model information, token counts, finish reasons, and accumulating response content and tool calls. The main callback interface with libcurl handles incoming data chunks and coordinates parsing, validation, and user callback invocation.

## Code

```
function yyjson_get_int_wrapper(val):
    convert JSON integer value to int64
    consolidates inline function expansion to single location

function extract_model(parent, event):
    validate event has "data: " prefix
    extract JSON payload after prefix
    if payload is "[DONE]" marker, return NULL

    parse JSON from payload
    if parse fails, return NULL

    validate root is JSON object
    if validation fails, free document and return NULL

    lookup "model" field in root object
    if model field missing or not string, free document and return NULL

    get string value from model field
    duplicate string into parent's memory context
    free JSON document

    return duplicated model string (or panic if allocation failed)

function extract_completion_tokens(event):
    validate event has "data: " prefix
    extract JSON payload after prefix
    if payload is "[DONE]" marker, return -1

    parse JSON from payload
    if parse fails, return -1

    validate root is JSON object
    if validation fails, free document and return -1

    lookup "usage" object in root
    if usage missing or not object, free document and return -1

    lookup "completion_tokens" field in usage
    if field missing or not integer, free document and return -1

    get integer value from completion_tokens field
    free JSON document

    return completion token count (or -1 if missing)

function extract_finish_reason(parent, event):
    validate event has "data: " prefix
    extract JSON payload after prefix
    if payload is "[DONE]" marker, return NULL

    parse JSON from payload
    if parse fails, return NULL

    validate root is JSON object
    if validation fails, free document and return NULL

    lookup "choices" array in root
    if choices missing, not array, or empty, free document and return NULL

    get first element from choices array
    if element missing or not object, free document and return NULL

    lookup "finish_reason" field in choice object
    if finish_reason missing or not string, free document and return NULL

    get string value from finish_reason field
    duplicate string into parent's memory context
    free JSON document

    return duplicated finish_reason string (or panic if allocation failed)

function http_write_callback(data, size, nmemb, userdata):
    cast userdata to write context structure

    calculate total bytes received (size * nmemb)

    feed incoming data to SSE parser

    extract all complete SSE events from parser:
        for each event:
            parse event to extract content
            if parse error, log and continue

            if content exists:
                invoke user's streaming callback (if provided)
                if callback returns error, mark context error and stop processing

                accumulate content to complete response buffer
                if accumulation fails, panic

                free content
            else (no content):
                attempt to parse tool calls from same event
                if tool call found:
                    if first tool call chunk, take ownership
                    else accumulate arguments to existing tool call

                free tool call if not kept

            extract model from event (only once)
            extract finish_reason from event (only once)
            extract completion_tokens from event (only once)

            free event

    return total bytes received to indicate success
```
