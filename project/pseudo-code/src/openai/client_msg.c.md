## Overview

This module provides factory functions for creating OpenAI message structures. It supports three message types: regular text messages, tool call messages (representing AI-invoked functions), and tool result messages (containing tool execution outcomes). Each message type has appropriate metadata stored in both human-readable fields and JSON structures.

## Code

```
function create_message(context, role, content):
    validate role and content are not empty

    allocate new message structure
    set message ID to 0 (in-memory message, not from database)

    set message role to the provided role string
    set message content to the provided content

    set message data_json to null (text messages have no structured data)

    return message

function create_tool_call_message(context, tool_id, call_type, function_name, json_arguments, summary):
    validate all parameters are not empty

    allocate new message structure
    set message ID to 0 (in-memory message, not from database)

    set message role to "tool_call"
    set message content to the provided summary

    create a JSON object with the following structure:
        - "id": tool_id
        - "type": call_type
        - "function": object containing:
            - "name": function_name
            - "arguments": json_arguments

    serialize JSON object to string
    store JSON string in message data_json

    return message

function create_tool_result_message(context, tool_call_id, result_content):
    validate tool_call_id and result_content are not empty

    allocate new message structure
    set message ID to 0 (in-memory message, not from database)

    set message role to "tool_result"
    set message content to the provided result content

    create a JSON object containing:
        - "tool_call_id": tool_call_id

    serialize JSON object to string
    store JSON string in message data_json

    return message
```
