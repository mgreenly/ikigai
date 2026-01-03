## Overview

This module provides HTTP client functionality for the OpenAI Chat Completions API. It manages the lifecycle of conversations by allowing messages to be added to a conversation context, serializing those conversations into JSON format for API requests, and handling responses from OpenAI's servers including both regular text responses and tool calls. The implementation uses streaming Server-Sent Events (SSE) for real-time response handling.

## Code

```
function get_message_at_index(messages, index):
    return the message at the given index


function create_conversation():
    allocate a new conversation structure
    initialize with empty message list
    return the conversation


function add_message_to_conversation(conversation, message):
    validate conversation and message exist

    resize the message array to accommodate one more message
    add the message to the end of the array
    increment message count

    transfer ownership of message to conversation (for cleanup)

    return success


function clear_conversation(conversation):
    validate conversation exists

    for each message in conversation:
        free the message

    free the messages array
    reset message list to empty state


function create_openai_request(config, conversation):
    validate config and conversation exist

    allocate a new request structure
    copy model name from config
    reference the conversation (borrowed, not owned)
    set temperature from config
    set max completion tokens from config
    enable streaming

    return the request


function create_openai_response():
    allocate a new response structure
    initialize content, finish reason to null
    initialize token counts to zero

    return the response


function serialize_request_to_json(request, tool_choice):
    validate request and conversation exist

    create a JSON document with talloc memory management
    create root object in JSON
    add model field to root

    create messages array

    for each message in the conversation:
        skip metadata events (not part of LLM conversation)

        if message is a tool_call:
            serialize tool call message format
        else if message is a tool_result:
            serialize tool result message format
        else:
            create message object with role and content
            add to messages array

    add messages array to root

    build tools array from all available tools
    add tools array to root

    add tool_choice field to request
    add stream field set to true
    add temperature field
    add max_completion_tokens field

    convert JSON document to string
    copy string to talloc context for proper lifetime management
    free yyjson-allocated string (different memory manager)

    return JSON string


function create_chat_completion(config, conversation, stream_callback, callback_context):
    validate config and conversation exist

    validate conversation has at least one message
    validate OpenAI API key is configured and not empty

    create request object from config and conversation
    serialize request to JSON with auto tool choice mode

    perform HTTP POST to OpenAI Chat Completions endpoint
    if HTTP request fails:
        return the error

    extract response data from HTTP response

    if response contains a tool call:
        extract tool call ID, name, and arguments
        create human-readable summary as "name(arguments)"
        create canonical tool_call message with all details
    else:
        create canonical assistant message with text content

    free HTTP response structure
    return the message as success result
```
