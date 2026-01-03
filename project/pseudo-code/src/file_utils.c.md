## Overview

This file provides a core utility for reading entire files into memory. The `ik_file_read_all` function opens a file, determines its size, allocates sufficient memory, reads the contents, and returns the buffer with proper null termination. Error handling covers all I/O failures and guards against integer overflow when allocating memory.

## Code

```
function ik_file_read_all(memory_context, file_path):
    validate inputs: memory context, file path, and output pointers all exist

    open file in binary read mode
    if open fails:
        return error indicating file could not be opened

    seek to end of file to determine size
    if seek fails:
        close file
        return error indicating seek failed

    query current file position (now at end)
    if position lookup fails:
        close file
        return error indicating size could not be determined

    seek back to beginning of file
    if seek fails:
        close file
        return error indicating seek failed

    cast file size to unsigned integer
    validate file size won't overflow when we add 1 for null terminator
    if file would be too large:
        close file
        return error indicating file is too large

    allocate buffer from memory context with space for file contents plus null terminator
    if allocation fails:
        close file
        panic (memory allocation failure is unrecoverable)

    read entire file contents into buffer
    close file

    verify all bytes were successfully read
    if read count doesn't match expected file size:
        return error indicating incomplete read

    null-terminate the buffer

    set output parameters: buffer pointer and optionally file size
    return success with buffer pointer
```
