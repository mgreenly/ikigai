# Task: Migrate debug infrastructure to Shared Context

## Target
Phase 0: Shared Context DI - Step 6 (debug fields migration)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/backlog/shared-context-di.md (design document)

## Pre-read Source (patterns)
- src/shared.h (current shared context)
- src/shared.c (current shared init)
- src/repl.h (debug fields)
- src/repl_init.c (debug initialization)
- src/debug_pipe.h (ik_debug_pipe_manager_t, ik_debug_pipe_t)

## Pre-read Tests (patterns)
- tests/unit/shared/shared_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `cfg`, `term`, `render`, `db_ctx`, `session_id`, `history` already migrated to shared context

## Task
Migrate debug infrastructure from `ik_repl_ctx_t` to `ik_shared_ctx_t`:
- `debug_mgr` - debug pipe manager
- `debug_enabled` - debug flag
- `openai_debug_pipe` - OpenAI debug pipe
- `db_debug_pipe` - database debug pipe

Debug infrastructure is shared across all agents. Even though the OpenAI code is not currently sending data to the pipe, we want to keep the concept of having pipes and enabling/disabling debug mode.

After this task:
- `ik_shared_ctx_init()` creates debug infrastructure
- Access pattern becomes `repl->shared->debug_mgr`, `repl->shared->debug_enabled`, etc.

## TDD Cycle

### Red
1. Update `src/shared.h`:
   - Add forward declarations:
     ```c
     typedef struct ik_debug_pipe_manager ik_debug_pipe_manager_t;
     typedef struct ik_debug_pipe ik_debug_pipe_t;
     ```
   - Add fields:
     ```c
     ik_debug_pipe_manager_t *debug_mgr;
     ik_debug_pipe_t *openai_debug_pipe;
     ik_debug_pipe_t *db_debug_pipe;
     bool debug_enabled;
     ```

2. Update `tests/unit/shared/shared_test.c`:
   - Test `shared->debug_mgr` is initialized (or NULL if debug disabled)
   - Test `shared->debug_enabled` reflects config
   - Test debug pipes are created when debug enabled

3. Run `make check` - expect failures

### Green
1. Update `src/shared.c`:
   - Add include for debug_pipe.h
   - Move debug initialization from repl_init.c:
     ```c
     shared->debug_enabled = cfg->debug_enabled;
     if (shared->debug_enabled) {
         shared->debug_mgr = ik_debug_pipe_manager_create(shared);
         if (shared->debug_mgr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

         shared->openai_debug_pipe = ik_debug_pipe_create(shared->debug_mgr, "openai");
         shared->db_debug_pipe = ik_debug_pipe_create(shared->debug_mgr, "db");
     }
     ```

2. Update `src/repl.h`:
   - Remove `ik_debug_pipe_manager_t *debug_mgr;` field
   - Remove `ik_debug_pipe_t *openai_debug_pipe;` field
   - Remove `ik_debug_pipe_t *db_debug_pipe;` field
   - Remove `bool debug_enabled;` field

3. Update `src/repl_init.c`:
   - Remove debug initialization (now in shared_ctx_init)

4. Update ALL files that access debug fields:
   - Change `repl->debug_mgr` to `repl->shared->debug_mgr`
   - Change `repl->debug_enabled` to `repl->shared->debug_enabled`
   - Change `repl->openai_debug_pipe` to `repl->shared->openai_debug_pipe`
   - Change `repl->db_debug_pipe` to `repl->shared->db_debug_pipe`

5. Run `make check` - expect pass

### Refactor
1. Verify debug cleanup is handled properly (via talloc hierarchy)
2. Verify no direct debug field access remains in repl_ctx
3. Run `make lint` - verify clean

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- Debug fields are in `ik_shared_ctx_t`, not `ik_repl_ctx_t`
- Debug infrastructure creation happens in shared_ctx_init
- All debug field access uses `repl->shared->*` pattern
- 100% test coverage maintained
- **All Phase 0 fields migrated**
