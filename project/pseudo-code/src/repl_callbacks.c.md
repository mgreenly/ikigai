## Overview

HTTP callback handlers for the REPL's OpenAI API integration. Processes streaming responses chunk-by-chunk, accumulates complete responses, and stores metadata from HTTP completions (success/failure status, model info, tool calls) for later persistence and display.

## Code

```
function streaming_callback(chunk, agent_context):
    validate chunk and context exist

    accumulate chunk into complete response buffer

    process chunk for line-based streaming display:
        scan chunk for newlines

        for each newline found:
            if streaming line buffer has content:
                combine buffered content with chunk prefix
                append complete line to scrollback
                release buffer
            else if chunk prefix is non-empty:
                append prefix directly to scrollback
            else:
                append empty line (blank line spacing)

        update scan position to after newline

    if remaining chunk content has no newline:
        append to streaming line buffer for next chunk

    return success (event loop handles rendering)


function http_completion_callback(completion, agent_context):
    validate completion and context exist

    log response metadata via JSON logger:
        record event type (success vs error)
        if success:
            record model name, finish reason, token count
        if tool call present:
            record tool call name and arguments

    flush any incomplete buffered line from streaming:
        if buffer exists, append to scrollback and clear

    if request succeeded:
        add blank line to scrollback (spacing after response)

    clear previous error message (if any)

    if request failed and error message present:
        store error message for display in UI

    if request succeeded:
        clear previous response metadata

        store new metadata for database persistence:
            model name
            finish reason
            token count

        if tool call present:
            create deep copy of tool call and store as pending

    return success
```
