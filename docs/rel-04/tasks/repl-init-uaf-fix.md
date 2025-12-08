# Task: Fix Use-After-Free Bug in repl_init.c

## Target
Bug Fix: Use-after-free in error paths of `ik_repl_init()`

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/di.md
- .agents/skills/patterns/arena-allocator.md

### Pre-read Docs
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md
- fix.md (full problem analysis)

### Pre-read Source (patterns)
- src/marks.c:62-66 (CORRECT pattern: error allocated on parent context)
- src/terminal.c:21-96 (ik_term_init - already correct, errors on parent)
- src/render.c:14-36 (ik_render_create - already correct, errors on parent)
- src/db/connection.c:63-80 (ik_db_init - already correct, errors on parent)

### Pre-read Source (bug sites)
- src/repl_init.c (current broken pattern with band-aid talloc_steal)

### Pre-read Tests (patterns)
- tests/unit/terminal/*.c (init failure tests)

## Pre-conditions
- `make check` passes
- Band-aid `talloc_steal(parent, result.err)` calls exist at:
  - Line 47: `ik_term_init` failure
  - Line 59: `ik_render_create` failure
  - Line 180: `ik_db_init_` failure
  - Line 188: `ik_repl_restore_session_` failure

## Task
Refactor `ik_repl_init()` to use the correct "allocate late" pattern:

1. Call sub-init functions with `parent` (not `repl`)
2. Only allocate `repl` context after all failable sub-inits succeed
3. Use `talloc_steal()` to reparent successful results under `repl`
4. Remove the band-aid `talloc_steal(parent, result.err)` calls

The principle: errors and successful sub-contexts are both children of `parent`. If a later init fails, earlier successful contexts are freed explicitly. Only when all inits succeed do we create `repl` and wire everything together.

## TDD Cycle

### Red
1. Add/verify tests for init failure scenarios:
   - `ik_term_init` fails: error message survives, no memory leak
   - `ik_render_create` fails: term cleaned up, error survives
   - `ik_db_init_` fails: term + render cleaned up, error survives

2. Run with ASan (`make BUILD=sanitize check`) to verify no UAF

### Green
1. Refactor `ik_repl_init()`:
   ```c
   res_t ik_repl_init(void *parent, ik_cfg_t *cfg, ik_repl_ctx_t **repl_out)
   {
       // Phase 1: Failable operations - all allocate on parent
       ik_term_ctx_t *term = NULL;
       res_t result = ik_term_init(parent, &term);
       if (is_err(&result)) {
           return result;  // Error on parent, safe
       }

       ik_render_ctx_t *render = NULL;
       result = ik_render_create(parent, term->screen_rows, term->screen_cols,
                                 term->tty_fd, &render);
       if (is_err(&result)) {
           ik_term_cleanup(term);
           talloc_free(term);
           return result;  // Error on parent, safe
       }

       // ... other failable inits ...

       // Phase 2: All succeeded - create repl and wire up
       ik_repl_ctx_t *repl = talloc_zero_(parent, sizeof(ik_repl_ctx_t));
       talloc_steal(repl, term);
       talloc_steal(repl, render);
       repl->term = term;
       repl->render = render;
       // ... wire up other components ...
   }
   ```

2. Remove all `talloc_steal(parent, result.err)` band-aid calls

3. Run `make BUILD=sanitize check` - expect pass with no ASan errors

### Refactor
1. Ensure cleanup order is correct (reverse of init order)
2. Verify terminal cleanup (`ik_term_cleanup`) is called before `talloc_free`
3. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- `make BUILD=sanitize check` passes (no ASan errors)
- No `talloc_steal(parent, result.err)` band-aid calls in `ik_repl_init()`
- Sub-init functions receive `parent`, not `repl`
- `repl` context allocated only after all failable inits succeed
- Error paths clean up successfully-allocated sub-contexts
- 100% test coverage maintained
