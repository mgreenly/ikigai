## Overview

Executes glob pattern matching to find files matching a wildcard pattern within an optional base path. Returns matching file paths as a newline-separated string along with the count of matches. Handles glob errors gracefully and reports them back to the caller.

## Code

```
function ik_tool_exec_glob(parent_memory_context, pattern, path):
    validate that pattern is not null

    build full pattern by combining path and pattern
    if path is provided and non-empty:
        full_pattern = path/pattern
    else:
        full_pattern = pattern

    execute glob matching with full pattern
    if glob fails with an error other than "no matches":
        determine error message based on error type:
            if out of memory: error = "Out of memory during glob"
            if read error: error = "Read error during glob"
            else: error = "Invalid glob pattern"

        build error response JSON
        free glob resources
        return error response

    if glob matched zero files:
        output = empty string
        count = 0
    else:
        count = number of matched files

        calculate total size needed to store all paths joined by newlines
        allocate output buffer

        concatenate each matched file path to output buffer
        join paths with newlines (no trailing newline)

    create result data with output string and file count
    build success response JSON containing:
        - output: the newline-separated matched paths
        - count: total number of matches

    free glob resources
    return success response
```
