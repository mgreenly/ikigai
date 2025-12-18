# Task: Standardize TALLOC_CTX parameter naming in scrollback module

## Target
Refactoring #4: Standardize Initialization Patterns (TALLOC_CTX Naming - Phase 2)

## Pre-read

### Skills
- scm
- di
- naming
- style
- tdd

### Source patterns
- src/scrollback.h
- src/scrollback.c

### Test patterns
- tests/unit/scrollback_*.c
- tests/**/test_scrollback*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `ik_scrollback_create` uses `void *parent` parameter name (current state)
- Phase 1 tasks (return semantics) have been completed

## What
Rename `void *parent` parameter to `TALLOC_CTX *ctx` in the scrollback module.

Per di.md and naming.md, the standard parameter name for talloc context is `TALLOC_CTX *ctx` everywhere.

Currently three different names are used across the codebase:
- `TALLOC_CTX *ctx` (array.c, agent.c, layer.c) - correct
- `TALLOC_CTX *mem_ctx` (db/*.c) - needs fixing in separate task
- `void *parent` (scrollback.c, terminal.c, render.c) - needs fixing

## How

### Step 1: Identify affected functions
In `src/scrollback.h`, the function using `void *parent`:
```c
ik_scrollback_t *ik_scrollback_create(void *parent, int32_t terminal_width);
```

### Step 2: Update function signature
Change:
```c
ik_scrollback_t *ik_scrollback_create(void *parent, int32_t terminal_width);
```
To:
```c
ik_scrollback_t *ik_scrollback_create(TALLOC_CTX *ctx, int32_t terminal_width);
```

### Step 3: Update function implementation
In `src/scrollback.c`, change:
```c
ik_scrollback_t *ik_scrollback_create(void *parent, int32_t terminal_width)
{
    // ... uses parent ...
}
```
To:
```c
ik_scrollback_t *ik_scrollback_create(TALLOC_CTX *ctx, int32_t terminal_width)
{
    // ... uses ctx ...
}
```

Replace all uses of `parent` with `ctx` within the function body.

### Step 4: Verify callers
Use sub-agent to find all callers:
```bash
grep -rn "ik_scrollback_create" src/ tests/
```

Callers should not need changes since they pass the same value regardless of parameter name. However, verify the call sites for consistency.

### Step 5: Verify
Run `make check` to ensure no compilation errors or test failures.

## Why
Consistent naming improves code readability and reduces cognitive load. When developers see `TALLOC_CTX *ctx` as the first parameter, they immediately understand it's a talloc memory context for ownership purposes.

The name `parent` is misleading because:
1. It suggests parent-child relationship semantics beyond memory ownership
2. It differs from the project standard (`ctx`)
3. It uses `void *` instead of the self-documenting `TALLOC_CTX` type

## TDD Cycle

### Red
N/A - existing tests serve as safety net for this refactoring.

### Green
Apply the parameter rename in header and implementation.

### Refactor
Verify with `make check` that all tests pass.

## Post-conditions
- `make check` passes with no errors
- `ik_scrollback_create` uses `TALLOC_CTX *ctx` parameter
- No occurrences of `void *parent` in scrollback.h or scrollback.c
- Working tree is clean (changes committed)

## Complexity
Low - mechanical parameter rename

## Notes
- `TALLOC_CTX` is a typedef for `void`, so the change is purely cosmetic/semantic
- The change documents intent: this parameter is a talloc allocation context
- This is part of a series of naming standardization tasks
