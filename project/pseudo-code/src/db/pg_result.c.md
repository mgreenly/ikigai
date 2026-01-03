## Overview

Provides a memory-safe wrapper around PostgreSQL query results. The wrapper ensures that PostgreSQL result objects are automatically cleaned up when their owning context is freed, using talloc's destructor mechanism to call the PostgreSQL cleanup function.

## Code

```
function wrap_pg_result(memory_context, pg_result):
    allocate a wrapper structure from the memory context

    if allocation fails:
        panic with out of memory error

    store the pg_result pointer in the wrapper

    register a destructor that will:
        check if the pg_result is still allocated
        if so, clear the pg_result using PostgreSQL's cleanup function
        mark the result as null

    return the wrapper

```
