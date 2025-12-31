---
name: pseudo-code
description: Guidelines for converting C source to explanatory pseudo-code
---

# Pseudo-Code

Guidelines for generating pseudo-code documentation in `project/pseudo-code/`.

## Purpose

Explain what code does, not how it's structured. Target audience: someone unfamiliar with C who wants to understand the behavior.

## File Format

```markdown
## Overview

One paragraph describing the file's responsibility.

## Code

Pseudo-code explanation here in a markdown code block.
```

**IMPORTANT:** The pseudo-code must be in a markdown code block (triple backticks).

## Example

```
function handle_resize(repl):
    query terminal for new dimensions (rows and columns)

    update render context with new dimensions

    recompute layout for scrollback area with new width
    recompute layout for input buffer with new width

    immediately redraw entire frame with new dimensions

    return success
```

This example demonstrates the desired flavor:
- Plain language explaining behavior
- No C syntax (no pointers, no struct access, no Result types)
- Logical grouping with blank lines
- Simple, readable flow

## Style Guidelines

- **Explain behavior, not syntax** - "validate the input" not "if (input == NULL) return ERR(...)"
- **Abstract C-isms** - No talloc, no Result types, no pointer arithmetic
- **Use plain language** - "return an error" not "return ERR(E_INVALID)"
- **Group related logic** - Don't mirror function boundaries if combining them explains better
- **Name concepts** - "the connection pool" not "conn_pool->entries[i]"

## Control Flow

- `if`, `for`, `while` are fine for simple cases
- Prefer iterator style when C loops are complex: `for each item in collection`
- Avoid index manipulation: `for i from 0 to len` → `for each item`

## Common Abstractions

| C Pattern | Pseudo-code |
|-----------|-------------|
| `if (x == NULL) return ERR(...)` | validate x exists |
| `talloc_new(ctx)` | allocate |
| `OK(value)` / `ERR(code)` | return success/failure |
| Complex `for` loops | for each item in collection |
| Simple `for` loops | for each / while (keep simple) |
| Pointer dereferencing | access the field |

## What to Preserve

- Business logic decisions
- Conditional branching that matters semantically
- Sequence of operations when order matters
- Error conditions worth noting
