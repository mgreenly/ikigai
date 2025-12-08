# Task: Migrate db_ctx and session_id to Shared Context

## Target
Phase 0: Shared Context DI - Step 4 (db_ctx + current_session_id migration)

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
- src/repl.h (db_ctx and current_session_id fields)
- src/repl_init.c (db initialization and session restore)
- src/db/connection.h (ik_db_ctx_t)
- src/db/session.h (session management)
- src/repl/session_restore.h (session restore logic)

### Pre-read Tests (patterns)
- tests/unit/shared/shared_test.c
- tests/unit/db/*.c (database test patterns)

## Pre-conditions
- `make check` passes
- `cfg`, `term`, `render` already migrated to shared context
- Database connection logic exists in repl_init.c

## Task
Migrate `db_ctx` and `current_session_id` fields from `ik_repl_ctx_t` to `ik_shared_ctx_t`. These represent the shared database session that all agents will use.

After this task:
- `ik_shared_ctx_init()` handles database connection (if configured)
- Access pattern becomes `repl->shared->db_ctx` and `repl->shared->session_id`

Note: Rename `current_session_id` to `session_id` for clarity (it's the session, not "current" vs something else).

## TDD Cycle

### Red
1. Update `src/shared.h`:
   - Add forward declaration: `typedef struct ik_db_ctx ik_db_ctx_t;`
   - Add fields:
     ```c
     ik_db_ctx_t *db_ctx;     // Database connection (NULL if not configured)
     int64_t session_id;       // Current session ID (0 if no database)
     ```

2. Update `tests/unit/shared/shared_test.c`:
   - Test shared_ctx with NULL db (no connection string in config)
   - Test shared->db_ctx is NULL when not configured
   - Test shared->session_id is 0 when not configured

3. Run `make check` - expect failures

### Green
1. Update `src/shared.c`:
   - Add includes for db/connection.h, db/session.h
   - Move database initialization from repl_init.c:
     ```c
     if (cfg->db_connection_string != NULL) {
         result = ik_db_init_(shared, cfg->db_connection_string, (void **)&shared->db_ctx);
         if (is_err(&result)) {
             // Cleanup already-initialized resources
             if (shared->term != NULL) ik_term_cleanup(shared->term);
             talloc_free(shared);
             return result;
         }
         // Session restore will happen in repl_init (needs conversation)
     }
     ```
   - Initialize session_id to 0 (session creation stays in repl_init for now)

2. Update `src/repl.h`:
   - Remove `ik_db_ctx_t *db_ctx;` field
   - Remove `int64_t current_session_id;` field

3. Update `src/repl_init.c`:
   - Remove database connection code (now in shared_ctx_init)
   - Keep session restore code but access via `repl->shared->db_ctx`
   - Update session restore to set `repl->shared->session_id`

4. Update `src/repl/session_restore.c` (and .h):
   - Change function to receive shared context or update access patterns
   - Session ID assignment updates shared->session_id

5. Update ALL files that access `repl->db_ctx` or `repl->current_session_id`:
   - Change `repl->db_ctx` to `repl->shared->db_ctx`
   - Change `repl->current_session_id` to `repl->shared->session_id`

6. Run `make check` - expect pass

### Refactor
1. Verify database cleanup is handled properly (via talloc or destructor)
2. Verify session restore still works correctly
3. Verify no direct db/session access remains in repl_ctx
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `db_ctx` and `session_id` are in `ik_shared_ctx_t`, not `ik_repl_ctx_t`
- Database connection happens in shared_ctx_init
- Session restore works with shared context
- All db/session access uses `repl->shared->*` pattern
- 100% test coverage maintained
