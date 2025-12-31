## Overview

Agent replay reconstruction for agent ancestry chains. Builds a chronological sequence of messages by walking backwards through an agent's parent hierarchy, identifying clear points (context resets) in each ancestor, and querying message ranges that bridge across fork boundaries.

## Code

```
function find_clear_event(agent_uuid, max_id):
    query the database for the most recent "clear" event for this agent
    (optionally limited by max_id if provided)

    if query fails:
        return error with database error message

    if a clear event exists:
        return its ID
    else:
        return 0 (no clear event found)


function build_replay_ranges(agent_uuid):
    ranges = empty list

    walk backwards from the given agent:
        find the most recent clear event before current agent's fork point

        if a clear was found:
            add range: [after_clear_id, fork_point] to ranges
            stop walking (clear terminates the ancestry chain)
        else:
            add range: [beginning_of_time, fork_point] to ranges

            get the current agent's record from the database

            if this agent has no parent:
                stop walking (root reached)
            else:
                move up to parent agent
                set fork_point to this agent's fork message ID

    reverse ranges to chronological order (root first)

    return the ranges


function query_message_range(agent_uuid, start_id, end_id):
    query the database for all messages from this agent
    where message ID is greater than start_id
    and (end_id is 0 OR message ID is less than or equal to end_id)
    order by creation time

    if query fails:
        return error with database error message

    if no messages found:
        return empty list

    for each row in the result:
        parse message ID, kind, content, and JSON data

        create a message object with parsed values

        add to result array

    return the array of messages


function replay_history(agent_uuid):
    build the chronological ranges for this agent's ancestry

    if building ranges fails:
        return error

    create an empty replay context

    for each range in chronological order:
        query the messages in that range

        if query fails:
            return error

        for each message in the result:
            copy message into the replay context
            grow context buffer if needed

    return the completed replay context
```
