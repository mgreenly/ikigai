## Overview

Restores agent state from the database on application startup. This module handles reconstructing the REPL context by loading all running agents, replaying their message history, populating conversation state and UI elements, and handling fresh installations. It ensures proper ordering (parents before children) and gracefully handles restoration errors.

## Code

```
function restore_all_agents(repl, database):
    allocate temporary memory for operations

    query database for all running agents
    if query fails:
        clean up and return error

    // Ensure proper ordering: oldest agents first (parents before children)
    sort agents by created_at timestamp in ascending order

    for each agent in sorted list:
        if agent has no parent:
            restore_root_agent(repl, database, temp_memory, agent_uuid)
        else:
            restore_child_agent(repl, database, agent)

    update navigation context for the current agent

    clean up temporary memory
    return success


function restore_root_agent(repl, database, temp_memory, agent_uuid):
    validate all inputs

    set up root agent reference in repl context

    // Reconstruct message history and UI state
    replay message history from database for this agent
    if replay fails:
        log warning and return early

    populate agent's conversation with replayed messages
    populate agent's scrollback (UI display) with events
    restore bookmarks/marks within the agent

    log success with message count and mark count

    // Handle first-time setup
    if no message history was found (fresh install):
        write initial clear event to database
        if system message is configured:
            write system message to database
            render system message in scrollback
            add system message to conversation state
        log fresh install completion


function restore_child_agent(repl, database, agent_row):
    validate all inputs

    allocate and initialize new agent context from database row
    if allocation fails:
        log warning, mark agent as dead, and return early

    set up repl reference in new agent

    // Reconstruct message history and UI state
    replay message history from database for this agent
    if replay fails:
        log warning, mark agent as dead, clean up, and return early

    populate agent's conversation with replayed messages
    populate agent's scrollback (UI display) with events
    restore bookmarks/marks within the agent

    // Register agent with REPL
    add agent to repl's agents list
    if addition fails:
        log warning, mark agent as dead, clean up, and return early

    log success with agent UUID, message count, and mark count


function compare_agents_by_creation_time(agent_a, agent_b):
    // Used by sort to order agents chronologically
    if agent_a created before agent_b:
        return -1
    if agent_a created after agent_b:
        return 1
    return 0


function handle_fresh_installation(repl, database):
    // Called when root agent has no message history

    write initial "clear" event to establish session start
    if write fails:
        log warning and continue

    if system message is configured in settings:
        write system message to database
        if write fails:
            log warning and continue
        else:
            render system message in UI scrollback
            if render fails:
                log warning and continue
            else:
                allocate system message object
                add system message to agent's conversation
                if addition fails:
                    log warning

    log fresh installation completion
```
