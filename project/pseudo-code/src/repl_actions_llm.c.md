## Overview

Handles user input processing in the REPL by routing messages either to the LLM for conversation or to the command dispatcher for slash commands. Manages the interaction flow including user message creation, persistence to the database, state transitions for waiting on LLM responses, and error handling.

## Code

```
handle_slash_command(repl, command):
    validate repl and command are not null
    assert command is the "pp" debug command

    create a format buffer for output
    pretty-print the input buffer contents into the format buffer
    extract the formatted output string
    append the output to the scrollback display (splitting by newlines)
    clean up the format buffer

    return success

handle_slash_cmd(repl, command_text):
    if command is the legacy "/pp" debug command:
        process with legacy handler
        if allocation fails, panic
    else:
        dispatch to the command dispatcher
        if dispatch fails:
            format error message
            append error message to scrollback
            free the error

send_to_llm(repl, message_text):
    create user message object from the text
    add message to current conversation
    if addition fails, panic

    if database connection exists and session is active:
        build JSON metadata with model, temperature, and token limit
        persist user message to database with metadata
        if persistence fails:
            log database error (suppress failure - non-critical)
        clean up metadata JSON

    clear any previous assistant response and streaming buffer
    reset tool iteration counter
    transition agent state to waiting for LLM response

    add LLM request to multi-request queue with streaming callbacks
    if request fails:
        append error message to scrollback
        transition agent back to idle state
        clean up error
    else:
        mark curl as still running (processing)

handle_newline_action(repl):
    validate repl is not null

    extract current input buffer text and length

    determine if input is a slash command (starts with /)
    if slash command:
        allocate buffer and copy command text

    dismiss any active completion suggestion

    if slash command:
        clear input buffer
        reset viewport offset
    else:
        submit the line to scrollback (display it)

    if slash command:
        dispatch the command
        clean up command buffer
    else if input is non-empty and conversation exists and config is loaded:
        allocate message buffer and copy input text
        send message to LLM
        clean up message buffer

    return success
```
