# Task: Standardize TALLOC_CTX parameter naming in db modules

## Target
Refactoring #4: Standardize Initialization Patterns (TALLOC_CTX Naming - Phase 2)

## Pre-read

### Skills
- scm
- di
- naming
- style
- tdd
- database

### Source patterns
- src/db/connection.h
- src/db/connection.c
- src/db/pg_result.h
- src/db/pg_result.c
- src/db/agent.h
- src/db/agent.c
- src/db/mail.h
- src/db/mail.c
- src/db/agent_replay.h
- src/db/agent_replay.c

### Test patterns
- tests/unit/db/*.c
- tests/integration/db/*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- Various db functions use `TALLOC_CTX *mem_ctx` parameter name (current state)
- Task `init-render-naming` has been completed

## What
Rename `TALLOC_CTX *mem_ctx` parameter to `TALLOC_CTX *ctx` in all db module functions.

Per di.md and naming.md, the standard parameter name for talloc context is `TALLOC_CTX *ctx` everywhere.

Currently three different names are used across the codebase:
- `TALLOC_CTX *ctx` (array.c, agent.c, layer.c) - correct
- `TALLOC_CTX *mem_ctx` (db/*.c) - needs fixing
- `void *parent` (scrollback.c, terminal.c, render.c) - fixed in previous tasks

## How

### Step 1: Identify all affected functions
Functions using `TALLOC_CTX *mem_ctx`:

**connection.h/c:**
- `ik_db_init`
- `ik_db_init_with_migrations`

**pg_result.h/c:**
- `ik_db_wrap_pg_result`

**agent.h/c:**
- `ik_db_agent_get`
- `ik_db_agent_list_running`
- `ik_db_agent_get_children`
- `ik_db_agent_get_parent`

**mail.h/c:**
- `ik_db_mail_inbox`
- `ik_db_mail_inbox_filtered`

**agent_replay.h/c:**
- `ik_agent_find_clear`
- `ik_agent_build_replay_ranges`
- `ik_agent_query_range`
- `ik_agent_replay_history`

### Step 2: Update all function signatures
For each function, change:
```c
res_t ik_db_*(..., TALLOC_CTX *mem_ctx, ...);
```
To:
```c
res_t ik_db_*(..., TALLOC_CTX *ctx, ...);
```

### Step 3: Update all function implementations
Replace all uses of `mem_ctx` with `ctx` within each function body.

### Step 4: Find and update callers
Use sub-agent to find all callers for verification:
```bash
grep -rn "mem_ctx" src/db/ tests/
```

Callers should not need changes since they pass the same value regardless of parameter name.

### Step 5: Verify
Run `make check` to ensure no compilation errors or test failures.

## Why
Consistent naming improves code readability and reduces cognitive load. When developers see `TALLOC_CTX *ctx` as the first or second parameter, they immediately understand it's a talloc memory context for ownership purposes.

The name `mem_ctx` is:
1. Unnecessarily verbose - `ctx` already implies memory context in this codebase
2. Inconsistent with other modules that use `ctx`
3. Creates cognitive load when reading code that mixes both naming styles

## TDD Cycle

### Red
N/A - existing tests serve as safety net for this refactoring.

### Green
Apply the parameter rename in all headers and implementations.

### Refactor
Verify with `make check` that all tests pass.

## Post-conditions
- `make check` passes with no errors
- All db functions use `TALLOC_CTX *ctx` parameter
- No occurrences of `mem_ctx` in src/db/*.h or src/db/*.c
- Working tree is clean (changes committed)

## Complexity
Medium - multiple files and functions to update, but mechanical changes

## Notes
- This affects 13 functions across 6 files (headers + implementations)
- `TALLOC_CTX` is a typedef for `void`, so the change is purely cosmetic/semantic
- The change documents intent: this parameter is a talloc allocation context
- Use find/replace carefully to avoid partial matches (e.g., variable names that happen to contain `mem_ctx`)
