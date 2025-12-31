## Overview

This file implements the initialization and discovery of the root agent (Agent 0) in the database. It ensures that a single root agent exists with no parent, creating it if necessary and adopting any orphaned messages that may exist from a migration period.

## Code

```
function ensure_agent_zero(database):
    create temporary context for query results

    query database for existing root agent (one with no parent)

    if query fails:
        clean up temporary context
        return error with database error message

    if root agent already exists:
        copy its UUID to output
        clean up temporary context
        return success with the UUID

    // No root agent exists - create one
    generate new UUID for Agent 0
    if UUID generation fails:
        clean up temporary context
        return out-of-memory error

    check if the messages table has agent_uuid column
    remember whether this column exists

    // Only check for orphaned messages if column was added by migration
    if agent_uuid column exists:
        query for any messages with no agent assigned
        record whether orphaned messages exist

    insert Agent 0 into agents table:
        - UUID: the newly generated UUID
        - name: NULL (no name for root agent)
        - parent_uuid: NULL (this is the root)
        - status: 'running' (start as active)
        - created_at: current timestamp
        - fork_message_id: 0 (not forked from anything)

    if insert fails:
        clean up temporary context
        return error with database error message

    // Adopt any orphaned messages from migration period
    if orphaned messages exist AND agent_uuid column exists:
        update all messages with no agent assigned to use Agent 0's UUID

        if update fails:
            clean up temporary context
            return error with database error message

    store Agent 0's UUID in output
    clean up temporary context
    return success
```
