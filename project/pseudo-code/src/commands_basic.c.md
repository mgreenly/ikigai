## Overview

This file implements five basic REPL commands used to manage the interactive session: `/clear` reinitializes the log and conversation state, `/help` displays available commands, `/model` switches the active AI model, `/system` sets the system prompt, and `/debug` toggles debug output mode. Each command updates the scrollback buffer for display and may persist changes to the session database.

## Code

```
function clear_command(context, repl, args):
    reinitialize the logger to rotate the current log file

    clear the scrollback buffer (displayed conversation history)
    clear the conversation (stored message context)

    free and reset all marks
    clear autocomplete state

    if database is available and session is active:
        persist a "clear" event to the database
        if system message is configured:
            persist the system message to the database

    if system message is configured:
        render the system message to the scrollback

    return success


function help_command(context, repl, args):
    append header "Available commands:" to scrollback

    retrieve all registered commands

    for each command:
        format as "  /name - description"
        append to scrollback

    return success


function model_command(context, repl, args):
    validate that model name is provided
        if missing, show error and return failure

    define list of valid models:
        gpt-4, gpt-4-turbo, gpt-4o, gpt-4o-mini, gpt-3.5-turbo,
        gpt-5, gpt-5-mini, o1, o1-mini, o1-preview

    validate model name against valid list
        if invalid, show error and return failure

    update config:
        free old model name
        allocate and store new model name

    show confirmation message in scrollback
    return success


function system_command(context, repl, args):
    free old system message

    if args is empty or null:
        set message to "System message cleared"
    else:
        allocate and store new system message
        set message to confirmation with new message text

    append message to scrollback
    return success


function debug_command(context, repl, args):
    if no arguments provided:
        show current debug status (ON or OFF)
    else if argument is "on":
        enable debug output
        show confirmation message
    else if argument is "off":
        disable debug output
        show confirmation message
    else:
        show error message for invalid argument
        return failure

    append result message to scrollback
    return success
```
