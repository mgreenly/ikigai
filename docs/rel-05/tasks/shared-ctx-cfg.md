# Task: Migrate cfg to Shared Context

## Target
Phase 0: Shared Context DI - Step 2 (cfg field migration)

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
- src/repl.h (ik_repl_ctx_t with cfg field)
- src/repl_init.c (cfg usage in init)
- src/config.h (ik_cfg_t definition)

## Pre-read Tests (patterns)
- tests/unit/shared/shared_test.c (existing shared tests)

## Pre-conditions
- `make check` passes
- `ik_shared_ctx_t` exists (empty struct)
- `ik_shared_ctx_init()` works

## Task
Migrate `cfg` field from `ik_repl_ctx_t` to `ik_shared_ctx_t`. The cfg pointer is borrowed (not owned) - it's loaded in main and passed via DI.

After this task:
- `ik_shared_ctx_init()` receives `ik_cfg_t *cfg` parameter
- `ik_repl_init()` receives `ik_shared_ctx_t *shared` instead of `ik_cfg_t *cfg`
- Access pattern becomes `repl->shared->cfg`

## TDD Cycle

### Red
1. Update `src/shared.h`:
   - Add forward declaration: `typedef struct ik_cfg ik_cfg_t;`
   - Add `ik_cfg_t *cfg;` field to struct
   - Update signature: `res_t ik_shared_ctx_init(TALLOC_CTX *ctx, ik_cfg_t *cfg, ik_shared_ctx_t **out);`

2. Update `tests/unit/shared/shared_test.c`:
   - Create mock/minimal cfg for tests
   - Test `ik_shared_ctx_init()` stores cfg pointer
   - Test `shared->cfg` is accessible

3. Run `make check` - expect failures (signature mismatch)

### Green
1. Update `src/shared.c`:
   - Add cfg parameter to init
   - Store cfg pointer in shared context
   - Assert cfg != NULL

2. Update `src/repl.h`:
   - Add forward declaration: `typedef struct ik_shared_ctx ik_shared_ctx_t;`
   - Add `ik_shared_ctx_t *shared;` field to struct
   - Remove `ik_cfg_t *cfg;` field (will access via shared->cfg)
   - Update signature: `res_t ik_repl_init(void *parent, ik_shared_ctx_t *shared, ik_repl_ctx_t **repl_out);`

3. Update `src/repl_init.c`:
   - Change parameter from `ik_cfg_t *cfg` to `ik_shared_ctx_t *shared`
   - Store `repl->shared = shared;`
   - Change all `cfg->` to `shared->cfg->`
   - Change all `repl->cfg->` to `repl->shared->cfg->`

4. Update `src/client.c` (main):
   - Create shared context before repl: `ik_shared_ctx_init(root_ctx, cfg, &shared)`
   - Pass shared to repl: `ik_repl_init(root_ctx, shared, &repl)`

5. Update ALL files that access `repl->cfg`:
   - Search for `repl->cfg` and change to `repl->shared->cfg`
   - This includes commands, event handlers, callbacks, etc.

6. Update test files that call `ik_repl_init`:
   **CRITICAL: The following files call `ik_repl_init` with the old signature and MUST be updated:**

   **Integration tests (pattern: create shared_ctx, pass to ik_repl_init):**
   - `tests/integration/completion_e2e_test.c` (8 calls)
   - `tests/integration/completion_edge_cases_test.c` (10 calls)
   - `tests/integration/history_lifecycle_test.c` (2 calls)
   - `tests/integration/repl_test.c` (1 call)

   **Unit tests that mock/create ik_repl_ctx_t manually:**
   - `tests/unit/repl/session_restore_errors_test.c` - update `create_test_repl()` helper
   - `tests/unit/repl/session_restore_test.c` - update `create_test_repl()` helper
   - `tests/unit/repl/session_restore_marks_test.c` - check if needs update

   **Update pattern for integration tests:**
   ```c
   // OLD:
   res_t res = ik_repl_init(ctx, cfg, &repl);

   // NEW:
   ik_shared_ctx_t *shared = NULL;
   res_t shared_res = ik_shared_ctx_init(ctx, cfg, &shared);
   ck_assert(is_ok(&shared_res));
   res_t res = ik_repl_init(ctx, shared, &repl);
   ```

   **Update pattern for unit test create_test_repl() helpers:**
   ```c
   static ik_repl_ctx_t *create_test_repl(TALLOC_CTX *ctx)
   {
       ik_repl_ctx_t *repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));

       // Create shared context with test config
       ik_cfg_t *cfg = ik_test_create_config(ctx);
       ik_shared_ctx_t *shared = NULL;
       res_t res = ik_shared_ctx_init(ctx, cfg, &shared);
       ck_assert(is_ok(&res));
       repl->shared = shared;

       repl->scrollback = ik_scrollback_create(repl, 80);
       // ... rest of initialization
       return repl;
   }
   ```

7. **CRITICAL: Run `make clean` before testing to avoid stale object files.**
   The struct layout has changed - stale .o files will cause SIGABRT/assertion failures.

8. Run `make check` - expect pass

### Refactor
1. Verify no direct cfg access remains in repl_ctx
2. Verify shared_ctx and repl_ctx are siblings under root_ctx
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes (after `make clean`)
- `cfg` is in `ik_shared_ctx_t`, not `ik_repl_ctx_t`
- `ik_repl_init()` receives shared context
- main() creates shared_ctx before repl_ctx
- All cfg access uses `repl->shared->cfg` pattern
- 100% test coverage maintained

## Known Issues (from previous attempt)

**Root cause of SIGABRT in session_restore_errors_test:**
The struct `ik_repl_ctx_t` was modified to add `ik_shared_ctx_t *shared` as the FIRST field.
When test object files (.o) are compiled with the OLD header (without shared field) but linked
against library code compiled with the NEW header, the struct field offsets are misaligned.

Example: Test sets `repl->scrollback` but the library reads `repl->term` at that offset.
This causes `ik_event_render(repl->scrollback, ...)` to receive NULL because `repl->scrollback`
is now at a different offset in the struct.

**Solution:**
1. Always run `make clean` before `make check` when struct layouts change
2. Ensure ALL test files are updated before running tests
3. The 21 ik_repl_init() call sites in tests/ listed above MUST be updated
