## Overview

Manages adding new OpenAI API requests to the multi-handle manager. This file handles the complete lifecycle of preparing a single request: validating inputs, creating request objects, serializing to JSON, configuring the HTTP connection with curl, and registering the request with the multi-handle manager for asynchronous processing.

## Code

```
function add_request_to_multi_handle(multi, config, conversation, stream_callback, stream_context, completion_callback, completion_context, limit_reached, logger):

    validate conversation contains at least one message
    validate API key exists and is not empty

    create request object from conversation configuration

    determine tool choice based on limit_reached flag:
        if limit_reached: set tool choice to none
        else: set tool choice to auto

    serialize request to JSON body with tool choice

    allocate active request context

    transfer ownership of JSON body to active request

    initialize curl easy handle
    if initialization fails:
        release active request
        return error

    allocate write callback context (for receiving response)
    if allocation fails:
        cleanup curl handle
        return error

    create SSE parser for streaming response
    initialize parser callbacks with user's stream callback
    initialize response buffer to empty

    store completion callback and its context

    configure curl POST request:
        set endpoint URL to OpenAI chat completions API
        set method to POST
        set request body to serialized JSON
        set write callback to receive response data
        set write context for callback

    build HTTP headers:
        add Content-Type: application/json
        build Authorization header with API key
        if header is too long:
            cleanup resources
            return error
        attach all headers to curl handle

    if logging is enabled:
        create log entry with event type "http_request"
        record method (POST), URL, and headers (without API key)
        parse request body JSON and include in log

    register easy handle with multi-handle manager
    if registration fails:
        cleanup curl handle and headers
        return error

    grow active requests array by one slot
    if array growth fails:
        deregister from multi-handle
        cleanup resources
        return error

    add request to active requests tracking array
    increment active request counter

    return success
```
