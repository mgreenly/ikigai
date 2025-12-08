# Task: Migrate term and render to Shared Context

## Target
Phase 0: Shared Context DI - Step 3 (term + render field migration)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/backlog/shared-context-di.md (design document)

## Pre-read Source (patterns)
- src/shared.h (current shared context with cfg)
- src/shared.c (current shared init)
- src/repl.h (term and render fields)
- src/repl_init.c (term and render initialization)
- src/terminal.h (ik_term_ctx_t)
- src/render.h (ik_render_ctx_t)

## Pre-read Tests (patterns)
- tests/unit/shared/shared_test.c

## Pre-conditions
- `make check` passes
- `cfg` already migrated to shared context
- `repl->shared->cfg` access pattern established

## Task
Migrate `term` and `render` fields from `ik_repl_ctx_t` to `ik_shared_ctx_t`. These are related (render depends on term dimensions) and represent shared I/O infrastructure.

After this task:
- `ik_shared_ctx_init()` creates term and render contexts
- Access pattern becomes `repl->shared->term` and `repl->shared->render`

Note: Terminal and render initialization moves from repl_init to shared_ctx_init. This is the "facade" pattern - shared_ctx creates its internal infrastructure.

## TDD Cycle

### Red
1. Update `src/shared.h`:
   - Add forward declarations:
     ```c
     typedef struct ik_term_ctx ik_term_ctx_t;
     typedef struct ik_render_ctx ik_render_ctx_t;
     ```
   - Add fields:
     ```c
     ik_term_ctx_t *term;
     ik_render_ctx_t *render;
     ```

2. Update `tests/unit/shared/shared_test.c`:
   - Test `shared->term` is initialized
   - Test `shared->render` is initialized
   - Test render dimensions match term dimensions

3. Run `make check` - expect failures

### Green
1. Update `src/shared.c`:
   - Add includes for terminal.h and render.h
   - Initialize term: `ik_term_init(shared, &shared->term)`
   - Initialize render: `ik_render_create(shared, term->screen_rows, term->screen_cols, term->tty_fd, &shared->render)`
   - Handle errors appropriately (return ERR, don't leave partial state)

2. Update `src/repl.h`:
   - Remove `ik_term_ctx_t *term;` field
   - Remove `ik_render_ctx_t *render;` field

3. Update `src/repl_init.c`:
   - Remove term initialization (now in shared_ctx_init)
   - Remove render initialization (now in shared_ctx_init)
   - Access via `repl->shared->term` and `repl->shared->render`

4. Update `src/client.c`:
   - Update `g_term_ctx_for_panic = repl->term;` to `g_term_ctx_for_panic = shared->term;`

5. Update ALL files that access `repl->term` or `repl->render`:
   - Change `repl->term` to `repl->shared->term`
   - Change `repl->render` to `repl->shared->render`

6. Add cleanup function `ik_shared_ctx_cleanup()` or use talloc destructor:
   - Call `ik_term_cleanup()` on shared->term

7. Update `ik_repl_cleanup()`:
   - Remove term cleanup (now handled by shared_ctx)

8. Run `make check` - expect pass

### Refactor
1. Verify term/render creation order is correct
2. Verify cleanup happens in correct order (render before term)
3. Verify no direct term/render access remains in repl_ctx
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `term` and `render` are in `ik_shared_ctx_t`, not `ik_repl_ctx_t`
- `ik_shared_ctx_init()` creates term and render
- Terminal cleanup handled by shared context
- All term/render access uses `repl->shared->*` pattern
- 100% test coverage maintained
