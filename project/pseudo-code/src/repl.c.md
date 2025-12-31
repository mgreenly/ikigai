## Overview

The REPL (Read-Eval-Print-Loop) module is the core event loop for the interactive terminal interface. It orchestrates the main event handling cycle, managing terminal input, network I/O for API communications via curl, tool execution on background threads, and rendering of all output. The REPL maintains responsive interactivity while managing asynchronous operations like long-running AI agent requests.

## Code

```
function run_repl_event_loop(repl):
    render initial frame to terminal

    initialize should_exit to false

    while not repl.quit and not should_exit:
        check if terminal was resized (SIGWINCH signal)

        set up file descriptor sets for select():
            - identify all file descriptors ready for reading/writing
            - find the maximum file descriptor number

        if debug output is enabled:
            add debug output pipes to the read file descriptor set

        calculate timeout for select() call:
            find minimum timeout needed for any pending curl transfers
            combine with animation timeout to determine select() timeout in milliseconds

        call select() to wait for I/O readiness or timeout:
            read_fds, write_fds, exception_fds
            timeout = effective_timeout_ms

        if select() was interrupted by signal:
            check for resize again
            continue to next loop iteration

        if select() returned error (other than interrupt):
            break event loop

        if select() timed out (ready == 0):
            handle timeout: update spinner animation and check for scrollback changes
            (note: do not skip curl event processing)

        if debug pipes have data ready:
            read from debug pipes and append to scrollback

        if terminal input is ready:
            read and process terminal input (keyboard, paste, etc.)
            if user requested exit (e.g., Ctrl+C twice):
                break event loop

        process any curl events from network transfers:
            for each agent with pending curl operations
            dispatch completed or failed requests

        check all background tool execution threads:
            if any tool execution completed, retrieve results and update agent state

        loop to next iteration


function submit_user_input_line(repl):
    extract text from current input buffer

    if text is not empty and history is available:
        add text to history (skip duplicate of last entry)

        if history file write fails:
            log warning but continue (do not block REPL)

        if currently browsing history:
            exit history browsing mode

    if text is not empty and scrollback exists:
        render text as a user message event to the scrollback

    clear the input buffer and reset viewport to bottom

    return success


function handle_terminal_resize(repl):
    query terminal for new dimensions (rows and columns)

    update render configuration with new rows and columns

    recompute layout for scrollback area with new column width
    recompute layout for input buffer with new column width

    immediately redraw entire frame with new dimensions

    return success


function should_agent_continue_tool_iteration(agent):
    if the last AI response finish_reason is not "tool_calls":
        return false

    if agent has exceeded maximum configured tool iterations:
        return false

    return true (continue tool loop)
```
