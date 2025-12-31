## Overview

This module handles the lifecycle management of database sessions. It provides three core operations: creating new sessions, retrieving the most recent active session, and ending sessions by marking their completion time. Sessions are stored in PostgreSQL with start and end timestamps, allowing the system to track when work began and ended.

## Code

```
function create_session(database_context):
    allocate temporary memory context for query result

    execute INSERT statement to create new session with default values
    return the newly created session ID

    check if query executed successfully
    if query failed:
        clean up temporary memory
        return error with database message

    validate we received exactly one row back

    extract the session ID from the returned row
    convert the ID string to a 64-bit integer

    clean up temporary memory
    store session ID in output parameter
    return success


function get_active_session(database_context):
    allocate temporary memory context for query result

    query for the most recent session where ended_at is null
    order by most recent start time, with ID as tiebreaker
    limit to one result

    check if query executed successfully
    if query failed:
        clean up temporary memory
        return error with database message

    if no results returned:
        clean up temporary memory
        set output parameter to 0 (no active session)
        return success

    extract the session ID from the returned row
    convert the ID string to a 64-bit integer

    clean up temporary memory
    store session ID in output parameter
    return success


function end_session(database_context, session_id):
    validate session_id is positive

    allocate temporary memory context for query result

    prepare UPDATE statement to set ended_at = current time
    for the session matching the provided session_id

    convert session ID to string format for query parameter

    execute parameterized query with session ID

    check if query executed successfully
    if query failed:
        clean up temporary memory
        return error with database message

    clean up temporary memory
    return success
```
