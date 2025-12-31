## Overview

Manages the shared runtime context for the ikigai REPL, providing centralized access to configuration, terminal state, database connection, command history, and debug infrastructure. Handles initialization and cleanup of all shared resources with proper error recovery.

## Code

```
function shared_destructor(shared_context):
    if terminal exists:
        restore terminal to original state
    return success


function initialize_shared_context(parent_memory_context, config, working_dir, ikigai_path, logger, output_reference):
    validate all inputs are provided

    allocate shared context structure
    if allocation fails:
        panic - out of memory

    store config reference
    store injected logger and transfer ownership to context

    initialize terminal (enables raw mode and alternate screen)
    if terminal init fails:
        cleanup and return error

    initialize renderer with terminal dimensions
    if renderer init fails:
        cleanup terminal and context
        return error

    if database connection string is configured:
        initialize database connection
        if connection fails:
            cleanup terminal and context
            return error
    else:
        set database connection to null

    initialize session ID to 0

    create command history with configured max size
    load history from persistent storage
    if history load fails:
        log warning but continue with empty history (graceful degradation)
        free error information

    create debug manager for infrastructure debugging
    if debug manager creation fails:
        panic - out of memory

    add named debug pipe for OpenAI debugging
    if pipe creation fails:
        panic - out of memory

    add named debug pipe for database debugging
    if pipe creation fails:
        panic - out of memory

    disable debug mode initially

    register destructor for automatic cleanup when context is freed

    store result in output reference
    return success with context reference
```
