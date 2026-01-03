## Overview

This module constructs JSON response objects for tool execution outcomes. It provides functions to build both error and success responses with optional custom fields or data payloads. All responses are created using memory-safe tallocated strings and follow a consistent JSON schema where success responses include an output field and error responses include an error message.

## Code

```
function tool_response_error(memory_context, error_message):
    create empty JSON document
    create root JSON object

    add "success" field set to false
    add "error" field with the provided error message

    serialize document to JSON string
    allocate result string from memory context
    free temporary JSON string and document

    return success with allocated JSON string

function tool_response_success(memory_context, output_string):
    create empty JSON document
    create root JSON object

    add "success" field set to true
    add "output" field with the provided output string

    serialize document to JSON string
    allocate result string from memory context
    free temporary JSON string and document

    return success with allocated JSON string

function tool_response_success_extended(memory_context, output_string, callback, callback_context):
    create empty JSON document
    create root JSON object

    add "success" field set to true
    add "output" field with the provided output string

    if callback is provided:
        invoke callback to add additional custom fields to the response

    serialize document to JSON string
    allocate result string from memory context
    free temporary JSON string and document

    return success with allocated JSON string

function tool_response_success_with_data(memory_context, data_populator, callback_context):
    create empty JSON document
    create root JSON object

    add "success" field set to true
    create a new nested "data" object

    invoke data_populator callback to populate the data object with fields

    attach the populated data object to the response

    serialize document to JSON string
    allocate result string from memory context
    free temporary JSON string and document

    return success with allocated JSON string
```
