## Overview

The agent module manages individual agent contexts within the ikigai system. Each agent represents a separate interactive conversation with its own UI state, input buffer, scrollback history, LLM interactions, and tool execution. This file provides lifecycle management (creation from scratch or restoration from database), conversation copying for agent forking, and thread-safe state transitions for coordinating between the UI event loop and background tool execution.

## Code

```
function agent_destructor(agent):
    acquire lock on tool thread mutex (for helgrind safety)
    release the lock
    destroy the mutex
    return success (allow talloc to free memory)

function create_new_agent(context, shared, parent_uuid):
    validate inputs not null

    allocate agent struct
    assign unique UUID to agent
    set agent name to unnamed (nil)
    set parent UUID if provided, otherwise nil
    store reference to shared state
    store repl reference as nil (set by caller later)
    record current time as creation timestamp

    determine terminal dimensions:
        if shared has terminal info, use those dimensions
        otherwise default to 80 columns × 24 rows

    initialize display state:
        create scrollback buffer with terminal width
        create layer cake (rendering stack) with terminal height

    initialize input state (preserves partial typed commands):
        create input buffer
        set separator visible
        set input buffer visible
        initialize input text to empty

    initialize tab completion:
        no completion widget yet (created on Tab press)

    initialize conversation state:
        create empty OpenAI conversation
        no marks yet (used for navigation)

    initialize LLM state:
        create curl_multi handle for HTTP requests
        set HTTP request counter to idle
        set state to IDLE
        clear assistant response
        clear streaming buffer
        clear HTTP error message
        clear response metadata (model, finish reason, token counts)

    initialize spinner animation:
        start at frame 0
        hide spinner initially

    create and add rendering layers:
        create scrollback layer (shows conversation history)
        add to layer cake
        create spinner layer (animated activity indicator)
        add to layer cake
        create separator layer (visual divider)
        add to layer cake
        create input layer (user input area)
        add to layer cake
        create completion layer (tab completion dropdown)
        add to layer cake

    initialize viewport offset to 0 (no scroll)

    initialize tool execution state:
        no pending tool call
        tool thread not running
        tool thread not complete
        no thread context
        no thread result
        zero iterations

    initialize tool thread mutex:
        if initialization fails:
            free agent memory
            return error

    register destructor to clean up mutex on memory free

    return created agent

function restore_agent_from_database(context, shared, database_row):
    validate inputs not null
    validate database row has UUID

    allocate agent struct

    restore identity from database (not generating new):
        copy UUID from database
        copy name from database (nil if not set)
        copy parent UUID from database (nil if not set)
        copy creation timestamp
        parse fork message ID if present
        store reference to shared state
        set repl reference to nil (set by caller later)

    determine terminal dimensions:
        if shared has terminal info, use those dimensions
        otherwise default to 80 columns × 24 rows

    initialize display state:
        create scrollback buffer with terminal width
        create layer cake with terminal height

    initialize input state:
        create input buffer
        set separator visible
        set input buffer visible
        initialize input text to empty

    initialize tab completion:
        no completion widget yet

    initialize conversation state:
        create empty conversation
        no marks yet

    initialize LLM state:
        create curl_multi handle
        set HTTP request counter to idle
        set state to IDLE
        clear all response fields

    initialize spinner:
        start at frame 0
        hide initially

    create and add rendering layers:
        (same as creation: scrollback, spinner, separator, input, completion)

    initialize viewport offset to 0

    initialize tool execution state:
        (same as creation)

    initialize tool thread mutex:
        if initialization fails:
            free agent memory
            return error

    register destructor

    return restored agent

function copy_conversation_from_parent_to_child(child_agent, parent_agent):
    validate child and parent not null
    validate both have conversations

    for each message in parent's conversation:
        create new message in child with same kind and content

        if parent message has JSON data:
            copy JSON data to new message
            if copy fails:
                return error

        add new message to child's conversation
        if add fails:
            return error

    return success

function check_if_agent_has_running_tools(agent):
    validate agent not null
    return whether tool thread is currently running

function transition_agent_to_waiting_for_llm(agent):
    validate agent not null

    acquire lock on tool thread mutex
    assert current state is IDLE
    change state to WAITING_FOR_LLM
    release lock

    show spinner animation
    hide input buffer (prevent user typing while LLM responds)

function transition_agent_to_idle(agent):
    validate agent not null

    acquire lock on tool thread mutex
    assert current state is WAITING_FOR_LLM
    change state to IDLE
    release lock

    hide spinner
    show input buffer (user can type again)

function transition_agent_to_executing_tool(agent):
    validate agent not null

    acquire lock on tool thread mutex
    assert current state is WAITING_FOR_LLM
    change state to EXECUTING_TOOL
    release lock

function transition_agent_from_executing_tool(agent):
    validate agent not null

    acquire lock on tool thread mutex
    assert current state is EXECUTING_TOOL
    change state back to WAITING_FOR_LLM
    release lock
```
