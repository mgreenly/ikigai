# Task: Standardize TALLOC_CTX parameter naming in render module

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
- src/render.h
- src/render.c

### Test patterns
- tests/unit/render_*.c
- tests/**/test_render*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `ik_render_create` uses `void *parent` parameter name (current state)
- Task `init-terminal-naming` has been completed

## What
Rename `void *parent` parameter to `TALLOC_CTX *ctx` in the render module.

Per di.md and naming.md, the standard parameter name for talloc context is `TALLOC_CTX *ctx` everywhere.

## How

### Step 1: Identify affected functions
In `src/render.h`, the function using `void *parent`:
```c
res_t ik_render_create(void *parent, int32_t rows, int32_t cols, int32_t tty_fd, ik_render_ctx_t **ctx_out);
```

### Step 2: Update function signature
Change:
```c
res_t ik_render_create(void *parent, int32_t rows, int32_t cols, int32_t tty_fd, ik_render_ctx_t **ctx_out);
```
To:
```c
res_t ik_render_create(TALLOC_CTX *ctx, int32_t rows, int32_t cols, int32_t tty_fd, ik_render_ctx_t **ctx_out);
```

### Step 3: Update function implementation
In `src/render.c`, change:
```c
res_t ik_render_create(void *parent, int32_t rows, int32_t cols,
                       int32_t tty_fd, ik_render_ctx_t **ctx_out)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE
    // ... uses parent ...
}
```
To:
```c
res_t ik_render_create(TALLOC_CTX *ctx, int32_t rows, int32_t cols,
                       int32_t tty_fd, ik_render_ctx_t **render_ctx_out)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    // ... uses ctx ...
}
```

Note: The output parameter `ctx_out` should be renamed to `render_ctx_out` to avoid confusion with the input `ctx` parameter.

Replace all uses of `parent` with `ctx` within the function body.

### Step 4: Verify callers
Use sub-agent to find all callers:
```bash
grep -rn "ik_render_create" src/ tests/
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
- `ik_render_create` uses `TALLOC_CTX *ctx` parameter
- No occurrences of `void *parent` in render.h or render.c
- Working tree is clean (changes committed)

## Complexity
Low - mechanical parameter rename

## Notes
- `TALLOC_CTX` is a typedef for `void`, so the change is purely cosmetic/semantic
- The change documents intent: this parameter is a talloc allocation context
- This is part of a series of naming standardization tasks
- Consider renaming the output parameter `ctx_out` to `render_ctx_out` to distinguish from the talloc context
