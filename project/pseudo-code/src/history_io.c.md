## Overview

Manages persistent storage and retrieval of command history entries. Handles reading history from a JSON Lines file (`.ikigai/history`), writing individual entries to disk, and loading historical commands into memory on startup. Implements file I/O operations with atomic writes (write-to-temp-then-rename) and recovers gracefully from malformed entries.

## Code

```
function ensure_history_directory():
    check if ".ikigai" directory exists

    if it exists and is a directory:
        return success

    if it exists but is not a directory:
        return error (file with same name exists)

    if it doesn't exist:
        attempt to create directory with 0755 permissions

        if directory already exists (race condition):
            return success

        if creation fails (permissions, disk full, etc.):
            return error

    return success


function parse_single_history_line(line):
    parse line as JSON

    if JSON is malformed:
        log warning (skip this line)
        return success with nothing

    extract root JSON object

    if root is not an object:
        log warning (skip this line)
        return success with nothing

    extract "cmd" field from object

    if "cmd" field missing or not a string:
        log warning (skip this line)
        return success with nothing

    return success with cmd value


function load_history(history_object):
    ensure history directory exists

    if directory creation failed:
        return error

    check if history file exists

    if file doesn't exist:
        create empty file
        return success (empty history)

    read entire file contents into memory

    if read failed:
        return error

    if file is empty:
        return success (empty history)

    split file contents by newlines

    for each line in file:
        if line is empty:
            skip

        parse line as history entry

        if parsing failed:
            return error

        if parsing returned nothing (malformed but skipped):
            skip

        if we have room in history:
            add command to temporary list

    if we collected more entries than capacity:
        keep only the most recent entries (discard oldest)

    for each retained entry:
        add to history object

    return success


function format_history_entry(command):
    get current time as UTC timestamp (ISO 8601 format)

    if time retrieval fails:
        return nothing

    create JSON object:
        "cmd": <command string>
        "ts": <timestamp>

    if JSON creation fails:
        return nothing

    serialize JSON object to string

    return JSON string


function save_history(history_object):
    create temporary context for memory allocation

    ensure history directory exists

    if directory creation failed:
        return error

    open temporary file for writing (".ikigai/history.tmp")

    if open failed:
        return error

    for each command in history:
        format command as JSON line

        if formatting failed:
            close file, delete temp file
            return error

        write JSON line to file with newline

        if write failed:
            close file, delete temp file
            return error

    close file

    atomically rename temp file to history file

    if rename failed:
        delete temp file
        return error

    clean up memory

    return success


function append_entry(command):
    create temporary context for memory allocation

    ensure history directory exists

    if directory creation failed:
        return error

    open history file in append mode (".ikigai/history")

    if open failed:
        return error

    format command as JSON line

    if formatting failed:
        close file
        return error

    write JSON line to file with newline

    if write failed:
        close file
        return error

    close file

    clean up memory

    return success
```
