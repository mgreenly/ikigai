## Overview

This module implements database operations for the mail system. It provides functions to insert mail messages, consume (pop) the next message from an inbox with optional sender filtering, check agent liveness, and fire PG NOTIFY for wake-up. All operations are scoped to a specific session. Messages are consumed on read — no separate delete operation.

## Code

```
function insert_mail(db, session_id, message):
    validate all inputs are provided

    create temporary memory context

    convert session_id and timestamp to string parameters

    prepare INSERT query with parameters: session_id, from_uuid, to_uuid, body, timestamp
    execute query to insert mail and retrieve the assigned ID

    if query execution fails:
        report error with database message
        return failure

    parse the returned ID and store it in the message

    // Wake any agent waiting for mail from this sender
    execute: NOTIFY agent_event_<to_uuid>

    cleanup temporary memory
    return success


function consume_next_message(db, context, session_id, to_uuid, from_uuid_filter):
    // Atomically fetch and delete the oldest matching message
    // from_uuid_filter may be NULL (match any sender)
    validate all inputs are provided

    create temporary memory context

    if from_uuid_filter is not NULL:
        prepare query:
            DELETE FROM mail
            WHERE id = (
                SELECT id FROM mail
                WHERE session_id = $1 AND to_uuid = $2 AND from_uuid = $3
                ORDER BY timestamp ASC
                LIMIT 1
            )
            RETURNING id, from_uuid, to_uuid, body, timestamp
        execute with parameters: session_id, to_uuid, from_uuid_filter
    else:
        prepare query:
            DELETE FROM mail
            WHERE id = (
                SELECT id FROM mail
                WHERE session_id = $1 AND to_uuid = $2
                ORDER BY timestamp ASC
                LIMIT 1
            )
            RETURNING id, from_uuid, to_uuid, body, timestamp
        execute with parameters: session_id, to_uuid

    if query execution fails:
        report error with database message
        return failure

    if no rows returned:
        return NULL (no message available)

    create message structure from returned row
    extract ID, from_uuid, to_uuid, body, timestamp

    cleanup temporary memory
    return message


function check_agent_alive(db, session_id, agent_uuid):
    // Check if an agent is alive (not dead)
    // Used by wait to detect dead agents during fan-in

    prepare query:
        SELECT status FROM agents
        WHERE uuid = $1 AND session_id = $2

    execute with parameters: agent_uuid, session_id

    if no rows returned:
        return false (agent not found = not alive)

    extract status from result
    return status == 'running'
```
