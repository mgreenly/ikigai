## Overview

This module implements a replay engine that reconstructs a message history by processing a sequence of events loaded from the database. It supports core message types (system, user, assistant, tool calls/results), control events (clear, mark, rewind), and maintains a mark stack to enable jumping back to labeled checkpoints. The replay algorithm processes events in order, building an in-memory context array, and also provides capacity-aware dynamic resizing for both the message array and mark stack.

## Code

```
function load_messages_from_database(context, database, session_id, logger):
    // Initialize empty context with zero capacity
    allocate context structure
    set context.messages = empty
    set context.count = 0
    set context.capacity = 0
    set context.mark_stack.count = 0
    set context.mark_stack.capacity = 0

    // Query all messages for this session, ordered by creation time
    execute "SELECT id, kind, content, data FROM messages WHERE session_id = ? ORDER BY created_at"
    if query fails
        return error with database error message

    // Process each event in chronological order
    for each row in query result:
        extract id, kind, content, data_json fields
        call process_event(context, id, kind, content, data_json, logger)

    return success with context


function process_event(context, id, kind, content, data_json, logger):
    // Clear: Remove all messages and marks from context
    if kind is "clear":
        reset context.count to 0
        reset context.mark_stack.count to 0
        return success

    // Regular message: Append to message array
    if kind is one of: "system", "user", "assistant", "tool_call", "tool_result":
        append_message(context, id, kind, content, data_json)
        return success

    // Mark: Record checkpoint with optional label
    if kind is "mark":
        append_message(context, id, kind, content, data_json)

        // Extract label from data_json if provided
        label = null
        if data_json exists:
            parse data_json as JSON
            if JSON contains "label" field and is a string:
                label = copy the label string

        // Push mark onto stack
        ensure mark stack has capacity for one more element
        add mark with:
            message_id = id
            label = label (may be null for auto-numbered marks)
            context_idx = current position in message array
        return success

    // Rewind: Jump back to a previous mark
    if kind is "rewind":
        if data_json is missing:
            log error: "Malformed rewind event: missing data field"
            return success (non-fatal)

        parse data_json as JSON
        if JSON parsing fails:
            log error: "Malformed rewind event: invalid JSON in data field"
            return success (non-fatal)

        extract target_message_id from JSON
        if target_message_id is missing or not an integer:
            log error: "Malformed rewind event: missing or invalid target_message_id"
            return success (non-fatal)

        // Find mark by its message_id
        find mark in mark_stack where mark.message_id equals target_message_id
        if mark not found:
            log error: "Invalid rewind event: target mark not found"
            return success (non-fatal)

        // Truncate context back to that mark
        set context.count = mark's context position + 1

        // Remove all marks after target mark from mark stack
        remove all marks after the target mark

        // Record the rewind event itself
        append_message(context, id, kind, content, data_json)
        return success

    // Unknown kind (should not happen with valid database)
    log error: "Unknown event kind"
    return success (non-fatal)


helper function append_message(context, id, kind, content, data_json):
    // Ensure array has room for one more message
    ensure_capacity(context)

    // Allocate message structure
    allocate message
    set message.id = id
    set message.kind = copy kind string
    set message.content = copy content string (or null if not provided)
    set message.data_json = copy data_json string (or null if not provided)

    // Add to array
    set context.messages[context.count] = message
    increment context.count
    return success


helper function ensure_capacity(context):
    // If array still has room, do nothing
    if context.count < context.capacity:
        return

    // Calculate new capacity with geometric growth
    if capacity is 0:
        new_capacity = 16 (INITIAL_CAPACITY)
    else:
        new_capacity = capacity * 2

    // Reallocate messages array with new capacity
    reallocate context.messages to new_capacity
    set context.capacity = new_capacity


helper function ensure_mark_stack_capacity(context):
    // If mark stack still has room, do nothing
    if context.mark_stack.count < context.mark_stack.capacity:
        return

    // Calculate new capacity with geometric growth
    if capacity is 0:
        new_capacity = 4 (MARK_STACK_INITIAL_CAPACITY)
    else:
        new_capacity = capacity * 2

    // Reallocate marks array with new capacity
    reallocate context.mark_stack.marks to new_capacity
    set context.mark_stack.capacity = new_capacity


helper function find_mark(context, target_message_id):
    // Search mark stack for mark with matching message_id
    for each mark in context.mark_stack:
        if mark.message_id equals target_message_id:
            return mark
    return not found
```
