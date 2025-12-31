## Overview

Implements a grep-like search tool that searches files matching a glob pattern for lines matching a regular expression. The tool compiles a regex pattern, expands a glob pattern to find files, iterates through each file to find matching lines, and returns results formatted as "filename:line_number: line_content". Results are returned as JSON with the matching lines and a count of matches found.

## Code

```
function grep_execute(pattern, glob_filter, search_path):
    validate pattern exists

    compile regular expression from pattern
    if pattern fails to compile:
        return error response with pattern error message

    determine search path (use current directory if not specified)

    build glob pattern by combining search_path with glob_filter
    if glob_filter is empty:
        search all files in path using "*" wildcard

    execute glob pattern to find matching files
    allocate output buffer for collecting matches
    initialize match counter

    for each file found by glob:
        validate file is a regular file (skip directories and special files)

        search_file(filename):
            open file for reading
            if file cannot be opened:
                skip to next file

            for each line in file:
                increment line number counter
                remove trailing newline from line

                test if line matches compiled regex pattern
                if line matches:
                    format match as "filename:line_number: line_content"
                    if this is not the first match:
                        prepend newline to separate from previous matches

                    append formatted match to output buffer
                    increment match counter

            close file and free line buffer

    cleanup glob results and compiled regex

    build JSON response with:
        output: collected matching lines
        count: total number of matches

    return success response
```
