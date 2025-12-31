## Overview

This module implements database operations for the mail system. It provides functions to insert mail messages into the database, retrieve a user's inbox with optional filtering by sender, mark messages as read, and delete messages. All operations are scoped to a specific session and validate that users can only access their own messages.

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

    cleanup temporary memory
    return success


function retrieve_inbox(db, context, session_id, to_uuid, output, count):
    validate all inputs are provided

    create temporary memory context

    prepare SELECT query to retrieve all mail for this session and recipient
    order results by read status (unread first) then by timestamp (newest first)

    execute query with parameters: session_id, to_uuid

    if query execution fails:
        report error with database message
        return failure

    get row count from results

    if no rows returned:
        return empty result

    allocate array of message pointers in caller's context

    for each row in results:
        create new message structure in the array

        extract ID, from_uuid, to_uuid, body from query results
        convert ID and timestamp from string to numeric values
        parse read flag (1 = read, 0 = unread)

        store message pointer in array

    return array of messages and count
    return success


function mark_message_read(db, mail_id):
    validate all inputs are provided

    create temporary memory context

    convert mail_id to string parameter

    prepare UPDATE query to set read=1 for the message
    execute query with parameter: mail_id

    if query execution fails:
        report error with database message
        return failure

    cleanup temporary memory
    return success


function delete_message(db, mail_id, recipient_uuid):
    validate all inputs are provided

    create temporary memory context

    convert mail_id to string parameter

    prepare DELETE query scoped to both mail_id and recipient_uuid
    execute query with parameters: mail_id, recipient_uuid

    if query execution fails:
        report error with database message
        return failure

    check how many rows were affected by the delete
    if no rows were affected (message doesn't exist or doesn't belong to recipient):
        report error "Mail not found or not yours"
        return failure

    cleanup temporary memory
    return success


function retrieve_inbox_filtered(db, context, session_id, to_uuid, from_uuid, output, count):
    validate all inputs are provided

    create temporary memory context

    prepare SELECT query to retrieve mail for this session, recipient, AND sender
    order results by read status (unread first) then by timestamp (newest first)

    execute query with parameters: session_id, to_uuid, from_uuid

    if query execution fails:
        report error with database message
        return failure

    get row count from results

    if no rows returned:
        return empty result

    allocate array of message pointers in caller's context

    for each row in results:
        create new message structure in the array

        extract ID, from_uuid, to_uuid, body from query results
        convert ID and timestamp from string to numeric values
        parse read flag (1 = read, 0 = unread)

        store message pointer in array

    return array of messages and count
    return success
```
