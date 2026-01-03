# Task: Standardize TALLOC_CTX parameter naming in terminal module

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
- src/terminal.h
- src/terminal.c

### Test patterns
- tests/unit/terminal_*.c
- tests/**/test_terminal*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `ik_term_init` uses `void *parent` parameter name (current state)
- Task `init-scrollback-naming` has been completed

## What
Rename `void *parent` parameter to `TALLOC_CTX *ctx` in the terminal module.

Per di.md and naming.md, the standard parameter name for talloc context is `TALLOC_CTX *ctx` everywhere.

## How

### Step 1: Identify affected functions
In `src/terminal.h`, the function using `void *parent`:
```c
res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out);
```

### Step 2: Update function signature
Change:
```c
res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out);
```
To:
```c
res_t ik_term_init(TALLOC_CTX *ctx, ik_term_ctx_t **ctx_out);
```

### Step 3: Update function implementation
In `src/terminal.c`, change:
```c
res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out)
{
    assert(parent != NULL);    // LCOV_EXCL_BR_LINE
    // ... uses parent ...
}
```
To:
```c
res_t ik_term_init(TALLOC_CTX *ctx, ik_term_ctx_t **ctx_out)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    // ... uses ctx ...
}
```

Replace all uses of `parent` with `ctx` within the function body.

### Step 4: Verify callers
Use sub-agent to find all callers:
```bash
grep -rn "ik_term_init" src/ tests/
```

Callers should not need changes since they pass the same value regardless of parameter name.

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
- `ik_term_init` uses `TALLOC_CTX *ctx` parameter
- No occurrences of `void *parent` in terminal.h or terminal.c
- Working tree is clean (changes committed)

## Complexity
Low - mechanical parameter rename

## Notes
- `TALLOC_CTX` is a typedef for `void`, so the change is purely cosmetic/semantic
- The change documents intent: this parameter is a talloc allocation context
- This is part of a series of naming standardization tasks
