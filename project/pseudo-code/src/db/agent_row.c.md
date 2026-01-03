## Overview

Parses a single row from a PostgreSQL query result into a structured agent record. Extracts and validates agent metadata including UUIDs, name, status, and timestamps, converting database string values into appropriate types.

## Code

```
function parse_agent_row(database_context, memory_context, query_result, row_index):

    allocate a new agent record in memory
    if allocation fails, panic with out of memory error

    // Extract required string fields
    extract uuid from column 0
    if uuid allocation fails, panic

    // Extract nullable name
    if column 1 is not null:
        extract name from column 1
        if name allocation fails, panic
    else:
        set name to empty

    // Extract nullable parent_uuid
    if column 2 is not null:
        extract parent_uuid from column 2
        if parent_uuid allocation fails, panic
    else:
        set parent_uuid to empty

    // Extract fork_message_id
    extract fork_message_id from column 3
    if allocation fails, panic

    // Extract status
    extract status from column 4
    if allocation fails, panic

    // Parse created_at timestamp (column 5)
    parse the created_at string as an integer
    if parsing fails, return error indicating invalid timestamp

    // Parse ended_at timestamp (column 6)
    parse the ended_at string as an integer
    if parsing fails, return error indicating invalid timestamp

    return the populated agent record with success
```
