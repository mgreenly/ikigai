## Overview

Database operations for agent lifecycle management. Provides functions to insert agents into the registry, track their status transitions, retrieve agent information by UUID or parent relationship, and query message history. Uses PostgreSQL with parameterized queries for safety and implements memory management through temporary contexts.

## Code

```
function insert_agent(database_context, agent_info):
    validate inputs

    create temporary memory context for query result

    construct parameterized SQL INSERT statement:
        - UUID, name, parent UUID, status='running', creation timestamp, fork message ID

    convert numeric timestamps to strings for SQL parameters

    execute parameterized query against database

    check query succeeded
    if failed:
        clean up memory
        return error with database message

    clean up memory
    return success


function mark_agent_dead(database_context, agent_uuid):
    validate inputs

    create temporary memory context for query result

    construct parameterized SQL UPDATE statement:
        - set status='dead', set ended_at to current timestamp
        - only update if current status is 'running' (idempotent)

    get current Unix timestamp and convert to string

    execute parameterized query against database

    check query succeeded
    if failed:
        clean up memory
        return error with database message

    clean up memory
    return success


function get_agent_by_uuid(database_context, memory_context, uuid):
    validate inputs

    create temporary memory context for query result

    construct parameterized SQL SELECT statement:
        - fetch UUID, name, parent UUID, fork message ID, status, created_at, ended_at
        - match by UUID

    execute parameterized query against database

    check query succeeded
    if failed:
        clean up memory
        return error with database message

    check if any rows returned
    if no rows:
        clean up memory
        return error (agent not found)

    parse the returned row into an agent record
    if parsing failed:
        clean up memory
        return parsing error

    clean up memory
    return success


function list_running_agents(database_context, memory_context):
    validate inputs

    create temporary memory context for query result

    construct SQL SELECT statement:
        - fetch all agent fields
        - filter for status='running'
        - order by creation time

    execute query against database

    check query succeeded
    if failed:
        clean up memory
        return error with database message

    get number of returned rows

    if no rows found:
        return success with empty result

    allocate array in caller's memory context to hold agent records
    if allocation fails:
        panic (out of memory)

    for each row in result set:
        parse row into an agent record
        if parsing failed:
            clean up memory
            return parsing error

    return success with array of agent records


function get_agent_children(database_context, memory_context, parent_uuid):
    validate inputs

    create temporary memory context for query result

    construct parameterized SQL SELECT statement:
        - fetch all agent fields
        - filter for matching parent UUID
        - order by creation time

    execute parameterized query against database

    check query succeeded
    if failed:
        clean up memory
        return error with database message

    get number of returned rows

    if no rows found:
        return success with empty result

    allocate array in caller's memory context to hold agent records
    if allocation fails:
        panic (out of memory)

    for each row in result set:
        parse row into an agent record
        if parsing failed:
            clean up memory
            return parsing error

    return success with array of child agent records


function get_agent_parent(database_context, memory_context, agent_uuid):
    validate inputs

    create temporary memory context for query result

    construct parameterized SQL SELECT statement:
        - join agents table to itself
        - select parent agent by matching child UUID's parent_uuid to parent UUID
        - fetch all parent agent fields

    execute parameterized query against database

    check query succeeded
    if failed:
        clean up memory
        return error with database message

    check if any rows returned
    if no rows:
        return success with NULL (agent has no parent / is root)

    parse the returned row into an agent record
    if parsing failed:
        clean up memory
        return parsing error

    clean up memory
    return success


function get_last_message_id(database_context, agent_uuid):
    validate inputs

    create temporary memory context for query result

    construct parameterized SQL query:
        - select MAX(id) from messages table
        - filter by matching agent UUID
        - coalesce NULL result to 0

    execute parameterized query against database

    check query succeeded
    if failed:
        clean up memory
        return error with database message

    extract message ID string from query result

    parse string to 64-bit integer
    if parsing fails:
        clean up memory
        return parse error

    clean up memory
    return success with message ID
```
