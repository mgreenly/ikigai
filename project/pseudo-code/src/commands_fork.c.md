## Overview

This file implements the `/fork` command, which creates a child agent as a copy of the current agent. The child inherits the parent's conversation history and scrollback display, but starts as a separate agent that can take different conversational paths. The command supports an optional prompt that immediately sends a message to the newly forked agent.

## Code

```
function parse_fork_prompt(args):
    if args is empty:
        return null

    if first character is not a quote:
        show error: "Prompt must be quoted"
        return empty string

    find closing quote
    if not found:
        show error: "Unterminated quoted string"
        return empty string

    extract text between quotes
    return prompt


function handle_fork_prompt(repl, prompt):
    create user message with the given prompt

    add message to current agent's conversation

    if database session exists:
        serialize current model configuration to JSON
        attempt to persist user message to database
        if persistence fails:
            log warning but continue

    render user message to current agent's scrollback

    clear any previous assistant response and streaming buffer from current agent

    reset tool iteration counter

    transition current agent to "waiting for LLM" state

    add LLM request to queue with streaming and completion callbacks
    if request fails:
        show error message
        transition current agent to idle state
    else:
        mark curl as still running


function fork_command(repl, args):
    wait for any currently running tools to complete

    parse optional prompt from arguments
    if parsing failed:
        return success (user already saw error message)

    check if fork is already in progress (atomic flag)
    if yes:
        show error: "Fork already in progress"
        return success

    set fork-in-progress flag

    begin database transaction
    if fails:
        clear fork flag
        return error

    record parent agent's last message ID (the fork point)
    if query fails:
        rollback transaction
        clear fork flag
        return error

    create child agent (new conversation context)
    if creation fails:
        rollback transaction
        clear fork flag
        return error

    set the repl backpointer on child agent

    store fork point message ID in child

    copy parent's conversation history to child
    if copy fails:
        rollback transaction
        clear fork flag
        return error

    copy parent's scrollback (visual history) to child
    if copy fails:
        rollback transaction
        clear fork flag
        return error

    insert child agent into database registry
    if insert fails:
        rollback transaction
        clear fork flag
        return error

    add child agent to repl's agent array
    if addition fails:
        rollback transaction
        clear fork flag
        return error

    if database session exists:
        create parent-side fork event with parent content and metadata
        include child UUID and fork point message ID
        persist parent fork event to database
        if persist fails:
            rollback transaction
            clear fork flag
            return error

        create child-side fork event with child content and metadata
        include parent UUID and fork point message ID
        persist child fork event to database
        if persist fails:
            rollback transaction
            clear fork flag
            return error

    commit database transaction
    if commit fails:
        clear fork flag
        return error

    switch repl's current agent to the child
    if switch fails:
        clear fork flag
        return error

    clear fork flag

    show confirmation message: "Forked from [parent UUID]" in child's scrollback

    if a prompt was provided and not empty:
        immediately handle the prompt (send to new agent and trigger LLM)

    return success
```
