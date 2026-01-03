## Overview

This file implements a memory allocator adapter that bridges yyjson's allocation interface with the project's talloc-based memory management system. It provides three functions that implement malloc, realloc, and free operations using talloc's hierarchical memory management, enabling JSON parsing and object creation within the project's memory ownership model.

## Code

```
function json_talloc_malloc(parent_context, size):
    allocate a block of memory of the given size
    attach the allocation as a child of the parent context
    return pointer to allocated memory

function json_talloc_realloc(parent_context, existing_pointer, old_size, new_size):
    note: old_size is not used (talloc tracks sizes internally)
    reallocate the existing memory block to the new size
    maintain parent context relationship
    return pointer to reallocated memory

function json_talloc_free(parent_context, pointer):
    note: parent_context is not needed (talloc_free is independent)
    free the memory block
    this also frees any children of the freed block

function create_talloc_allocator(parent_context):
    create an allocator structure with three function pointers:
        - malloc operation → json_talloc_malloc
        - realloc operation → json_talloc_realloc
        - free operation → json_talloc_free
        - parent context reference
    return the populated allocator structure

    the caller can pass this allocator to yyjson
    any JSON objects created will be managed under the parent context
    when the parent context is freed, all JSON objects are automatically freed
```
