## Overview

This file provides serialization helpers that transform internal message representations into OpenAI API wire format. It handles two critical message transformations: converting internal tool_call messages to OpenAI's assistant role format with a tool_calls array, and converting tool_result messages to OpenAI's tool role format with tool_call_id and content fields.

## Code

```
function serialize_tool_call_message(mutable_doc, message_object, internal_message, parent_context):
    validate all input parameters exist

    set message role to "assistant"

    validate internal_message has serialized JSON data

    allocate temporary memory context for parsing

    parse the internal JSON data as a JSON document
    validate parsing succeeded
    validate root is a valid object

    extract required fields from parsed JSON:
        - call ID
        - call type
        - function object (must be valid)

    validate all extracted fields exist

    extract function details from function object:
        - function name
        - function arguments as JSON string

    validate function details exist

    duplicate extracted strings into temporary memory to preserve them
    free the parsed JSON document
    validate string duplication succeeded

    create new mutable array for tool_calls
    add empty object to tool_calls array

    populate tool call object:
        add duplicated call ID field
        add duplicated type field

    create new function object
    populate function object:
        add duplicated function name
        add duplicated function arguments

    attach function object to tool call
    attach tool_calls array to message object

    cleanup temporary memory context

    complete serialization


function serialize_tool_result_message(mutable_doc, message_object, internal_message, parent_context):
    validate all input parameters exist

    set message role to "tool"

    validate internal_message has serialized JSON data

    allocate temporary memory context for parsing

    parse the internal JSON data as a JSON document
    validate parsing succeeded
    validate root is a valid object

    extract tool_call_id from parsed JSON
    validate tool_call_id exists

    duplicate tool_call_id into temporary memory
    free the parsed JSON document
    validate duplication succeeded

    add tool_call_id field to message
    add content field to message (from internal_message)

    cleanup temporary memory context

    complete serialization
```
