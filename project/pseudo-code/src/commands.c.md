## Overview

This file implements the REPL command registry and dispatcher. It maintains a static registry of available commands (each with a name, description, and handler function), provides a function to retrieve all commands, and implements a dispatcher that parses user input, looks up the matching command, executes its handler, and optionally persists the command execution to the database.

## Code

```
constants:
    commands = registry of 15 built-in commands:
        - clear, mark, rewind (session management)
        - fork, kill, send (agent lifecycle and communication)
        - check-mail, read-mail, delete-mail, filter-mail (message handling)
        - agents (display agent hierarchy)
        - help, model, system, debug (utility commands)

function get_all_commands() -> (registry, count):
    return the static command registry and total command count

function dispatch_command(context, repl, input):
    validate that input starts with "/"

    skip the leading "/" and any whitespace

    if command string is empty:
        display error message
        return error result

    parse the command name (text before next space)
    extract any remaining text as arguments

    remember current scrollback line count

    for each command in registry:
        if command name matches:
            execute the command handler with arguments

            if handler succeeded:
                persist the command to database:
                    - record input line
                    - record all new output lines added to scrollback
                    - record command metadata (name and arguments)

            return the handler's result

    if no matching command found:
        display "unknown command" error message
        return error result

function persist_command_to_database(context, repl, input, command_name, arguments, lines_before):
    skip if database is not available

    get current scrollback line count after command execution

    build content buffer containing:
        - the original input line
        - all new output lines (from lines_before to current count)

    build metadata JSON containing:
        - command name
        - command arguments (or null if none)

    insert record into database:
        - type: "command"
        - content: input + output
        - data: metadata JSON

    if database insertion fails:
        log warning (do not crash; memory state is authoritative)
```
