# Task: Migrate history to Shared Context

## Target
Phase 0: Shared Context DI - Step 5 (history field migration)

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

### Pre-read Docs
- docs/backlog/shared-context-di.md (design document)

### Pre-read Source (patterns)
- src/shared.h (current shared context)
- src/shared.c (current shared init)
- src/repl.h (history field)
- src/repl_init.c (history initialization)
- src/history.h (ik_history_t)
- src/history.c (history implementation)

### Pre-read Tests (patterns)
- tests/unit/shared/shared_test.c
- tests/unit/history/*.c

## Pre-conditions
- `make check` passes
- `cfg`, `term`, `render`, `db_ctx`, `session_id` already migrated to shared context
- History implementation exists from rel-04

## Task
Migrate `history` field from `ik_repl_ctx_t` to `ik_shared_ctx_t`. Command history is shared across all agents (user arrow-up history is global).

After this task:
- `ik_shared_ctx_init()` creates and loads history
- Access pattern becomes `repl->shared->history`

This is the final field migration for Phase 0.

## TDD Cycle

### Red
1. Update `src/shared.h`:
   - Add forward declaration: `typedef struct ik_history ik_history_t;`
   - Add field: `ik_history_t *history;`

2. Update `tests/unit/shared/shared_test.c`:
   - Test `shared->history` is initialized
   - Test history capacity matches config

3. Run `make check` - expect failures

### Green
1. Update `src/shared.c`:
   - Add include for history.h
   - Create history: `shared->history = ik_history_create(shared, (size_t)cfg->history_size);`
   - Load history:
     ```c
     result = ik_history_load(shared, shared->history);
     if (is_err(&result)) {
         // Log warning but continue with empty history (graceful degradation)
         ik_log_warn("Failed to load history: %s", result.err->msg);
         talloc_free(result.err);
     }
     ```

2. Update `src/repl.h`:
   - Remove `ik_history_t *history;` field

3. Update `src/repl_init.c`:
   - Remove history creation and loading (now in shared_ctx_init)

4. Update ALL files that access `repl->history`:
   - Change `repl->history` to `repl->shared->history`
   - This likely includes input handling for arrow up/down

5. Run `make check` - expect pass

### Refactor
1. Verify history file path comes from config correctly
2. Verify history is saved on shutdown (check existing save logic)
3. Verify no direct history access remains in repl_ctx
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `history` is in `ik_shared_ctx_t`, not `ik_repl_ctx_t`
- History creation and loading happens in shared_ctx_init
- All history access uses `repl->shared->history` pattern
- 100% test coverage maintained
