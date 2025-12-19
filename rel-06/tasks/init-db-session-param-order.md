# Task: Fix ik_db_session_create parameter order for DI

## Target
Refactoring #4: Standardize Initialization Patterns (DI Violations - Phase 3)

## Pre-read

### Skills
- scm
- di
- errors
- naming
- style
- tdd
- database

### Source patterns
- src/db/session.h
- src/db/session.c

### Test patterns
- tests/unit/db/session_*.c
- tests/integration/db/session_*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `ik_db_session_create` takes `ik_db_ctx_t *db_ctx` as first parameter (current state)
- Task `init-mark-decouple` has been completed (or documented as deferred)

## What
Evaluate and optionally fix the parameter order of `ik_db_session_create` for DI compliance.

Current signature:
```c
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
```

The refactor.md claims this violates DI because "First param is db_ctx, not TALLOC_CTX".

However, upon analysis, this may NOT be a violation because:
1. The function doesn't allocate output objects that need talloc context
2. It returns a session_id (primitive) via output parameter
3. The db_ctx IS the appropriate context for database operations

## How

### Step 1: Analyze the function
Looking at `ik_db_session_create` implementation:
- Creates temporary talloc context internally: `talloc_new(NULL)`
- Executes SQL query
- Extracts session_id primitive
- Cleans up temporary context
- Returns session_id via output parameter

The function doesn't need a talloc context because:
- No objects are allocated for the caller
- Errors are allocated on db_ctx (which is correct per error context lifetime rules)

### Step 2: Verify against DI principles
Per di.md:
- "First param is TALLOC_CTX - Explicit memory ownership"

But this applies when:
- The function allocates objects owned by the caller
- The caller needs to specify memory ownership

For `ik_db_session_create`:
- Returns primitive (int64_t), not allocated object
- No memory ownership to specify

### Step 3: Decision
Option A - No change needed:
The current signature is correct. The function returns a primitive, not an allocated object. The db_ctx is the appropriate first parameter for database operations.

Document this conclusion in the task output.

Option B - Add TALLOC_CTX for consistency:
```c
res_t ik_db_session_create(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t *session_id_out);
```

This would be purely for consistency, but introduces unnecessary complexity since the ctx would only be used for error allocation (which already works via db_ctx).

### Step 4: Verify
If making changes, run `make check` to ensure no compilation errors or test failures.

## Why
The original refactor.md identified this as a DI violation, but upon analysis:
- Functions returning primitives don't need TALLOC_CTX
- The db_ctx is the appropriate context for database operations
- Error allocation uses db_ctx as the allocation context

Per errors.md: "Error allocation context must survive the error's return" - db_ctx satisfies this requirement.

## TDD Cycle

### Red
N/A - this is analysis task

### Green
Document conclusion or apply fix if needed.

### Refactor
Verify with `make check` if any changes made.

## Post-conditions
- `make check` passes with no errors (if changes made)
- Decision documented (either "no change needed" or "changed to X")
- Working tree is clean (changes committed or no changes needed)

## Complexity
Low - analysis task, likely no code changes

## Notes
- Similar analysis applies to `ik_db_session_get_active` and `ik_db_session_end`
- All three functions return primitives via output parameters
- The pattern db_ctx as first param is consistent across all session functions
- Use sub-agents to find all callers if changes are made:
  ```bash
  grep -rn "ik_db_session_create\|ik_db_session_get_active\|ik_db_session_end" src/ tests/
  ```
