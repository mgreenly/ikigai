## Overview

This file implements the mark and rewind command handlers for the REPL. The `mark` command creates labeled checkpoints in the session history that can be stored in the database. The `rewind` command navigates back to a previously created mark, reverting the session state and persisting the rewind action to the database for audit purposes.

## Code

```
helper function get_mark_db_id(context, repl, label):
    if database is not connected or session is invalid:
        return empty result

    if a specific label is provided:
        query the database for the most recent mark message with that label
        parameters: current session ID, mark label
    else:
        query the database for the most recent mark message of any label
        parameters: current session ID

    execute the query
    if the result contains data:
        parse the message ID from the result
        return the message ID
    else:
        return empty result


function ik_cmd_mark(context, repl, args):
    validate inputs are non-null

    // Extract optional label from command arguments
    label = args (may be NULL for auto-numbered marks)

    // Create the mark in memory
    result = create mark in repl with the label
    if creation failed:
        return error

    // Persist mark event to database
    if database is connected and session is valid:
        if label is provided:
            build JSON data containing the label
        else:
            build empty JSON data

        insert a "mark" message record into the database with the JSON data
        if database insert fails:
            log warning about persistence failure
            // continue anyway - memory state is the source of truth
            clean up error

        free the JSON data

    return success


function ik_cmd_rewind(context, repl, args):
    validate inputs are non-null

    // Extract optional label from command arguments
    label = args (may be NULL to rewind to most recent mark)

    // Find the target mark by label (or use most recent)
    find result = locate mark in repl by label
    if find failed:
        show error message in scrollback display
        free error details
        return success  // suppress the error from bubbling up

    save the target mark's label for later use

    // Query database to get the message ID of the target mark
    // (needed to record what we're rewinding to)
    target_message_id = get_mark_db_id(context, repl, target_label)

    // Rewind the session to the target mark's state
    result = rewind repl to the target mark
    if rewind failed:
        clean up the label copy
        return error

    // Persist rewind event to database
    if database is connected, session is valid, and we have a valid target message ID:
        build JSON data with:
            - target message ID
            - target label (may be null if rewinding to unlabeled mark)

        insert a "rewind" message record into the database with the JSON data
        if database insert fails:
            log warning about persistence failure
            // continue anyway - memory state is the source of truth
            clean up error

        free the JSON data

    free the label copy
    return success
```
