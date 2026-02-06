## Overview

This module manages message persistence and creation in the database. It provides functions to validate message event kinds, insert messages into the messages table with parametrized queries, and construct tool result messages with structured JSON metadata.

## Code

```
constants:
    VALID_KINDS = ["clear", "system", "user", "assistant", "tool_call", "tool_result", "mark", "rewind", "agent_killed", "command", "fork", "capture"]

function is_valid_kind(kind):
    validate kind is not null

    for each valid_kind in VALID_KINDS:
        if kind matches valid_kind:
            return true

    return false


function insert_message(database, session_id, agent_uuid, kind, content, data_json):
    validate database context exists
    validate database connection exists
    validate session_id is positive
    validate kind is a valid event kind

    allocate temporary context for query cleanup

    build parameterized SQL query:
        INSERT INTO messages (session_id, agent_uuid, kind, content, data)
        VALUES ($1, $2, $3, $4, $5)

    convert session_id to string parameter

    prepare parameter array:
        [session_id_str, agent_uuid, kind, content, data_json]

    execute parameterized query with automatic result cleanup

    if query execution failed:
        capture PostgreSQL error message
        allocate error on database context (not temporary context, to avoid dangling pointer)
        free temporary context and its resources
        return error

    free temporary context and its resources
    return success


function create_tool_result_message(parent_context, tool_call_id, name, output, success, content):
    validate tool_call_id exists
    validate name exists
    validate output exists
    validate content exists

    allocate message structure in parent context

    set message id to 0 (in-memory message, not yet persisted)
    set message kind to "tool_result"
    set message content to human-readable summary

    allocate new JSON document
    create JSON object as document root

    add to JSON object:
        tool_call_id: value
        name: value
        output: value
        success: boolean value

    serialize JSON object to string
    copy JSON string into message structure (owned by message)
    free temporary JSON resources

    return message structure
```
