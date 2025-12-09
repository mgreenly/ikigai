# Task: Shared Context Cleanup and Verification

## Target
Phase 0: Shared Context DI - Final Verification

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/tdd.md
- .agents/skills/style.md

## Pre-read Docs
- docs/backlog/shared-context-di.md (success criteria section)

## Pre-read Source (patterns)
- src/shared.h (complete shared context)
- src/shared.c (complete shared init)
- src/repl.h (repl context with shared pointer)
- src/repl_init.c (repl init receiving shared)
- src/client.c (main with both contexts)

## Pre-read Tests (patterns)
- tests/unit/shared/*.c
- tests/helpers/test_contexts.h

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- All Phase 0 field migrations complete
- Test helpers created

## Task
Final cleanup and verification for Phase 0 completion:
1. Ensure no stale field references remain
2. Verify ownership hierarchy is correct (shared and repl are siblings)
3. Add documentation comments
4. Verify all success criteria from backlog are met

This is a verification/polish task, not new implementation.

## TDD Cycle

### Red (Verification)
1. Search codebase for any remaining direct field access:
   - `repl->cfg` (should be `repl->shared->cfg`)
   - `repl->term` (should be `repl->shared->term`)
   - `repl->render` (should be `repl->shared->render`)
   - `repl->db_ctx` (should be `repl->shared->db_ctx`)
   - `repl->current_session_id` (should be `repl->shared->session_id`)
   - `repl->history` (should be `repl->shared->history`)

2. Verify no circular includes:
   - shared.h should only have forward declarations
   - No header should include both shared.h and the concrete type headers

3. Run `make check` - should already pass

### Green (Cleanup)
1. Fix any remaining stale references found in verification

2. Add documentation to `src/shared.h`:
   ```c
   /**
    * Shared infrastructure context for ikigai.
    *
    * Contains resources shared across all agents in a session:
    * - Configuration (borrowed, not owned)
    * - Terminal I/O
    * - Database connection
    * - Command history
    *
    * Ownership: Created as sibling to ik_repl_ctx_t under root_ctx.
    * This follows DI principles: dependencies created first, injected
    * into consumers (repl_ctx).
    *
    * Thread safety: Currently single-threaded. Phase 2 will add
    * synchronization for multi-agent access.
    */
   ```

3. Add documentation to `ik_repl_ctx_t` in `src/repl.h`:
   ```c
   // Shared infrastructure (DI - not owned, just referenced)
   // See shared.h for what's available via this pointer
   ik_shared_ctx_t *shared;
   ```

4. Verify main() initialization order matches design:
   ```c
   // 1. Load config
   // 2. Create shared context (receives config, creates infrastructure)
   // 3. Create REPL (receives shared context)
   // 4. Run
   // 5. Cleanup (reverse order via talloc)
   ```

5. Run `make check` - expect pass

### Refactor
1. Run full quality checks:
   - `make lint` - no violations
   - `make coverage` - 100% coverage
   - Valgrind/sanitizers - no memory errors

2. Review Phase 0 success criteria from backlog:
   - [ ] `ik_shared_ctx_t` struct defined with forward declarations
   - [ ] `ik_shared_ctx_init()` creates infrastructure from config
   - [ ] `ik_repl_init()` receives shared context via DI
   - [ ] All shared fields migrated from repl_ctx
   - [ ] Clear ownership: shared_ctx and repl_ctx are siblings under root_ctx
   - [ ] No global state introduced
   - [ ] Field access uses `repl->shared->field` pattern

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- `make lint` passes
- `make coverage` shows 100%
- No stale field references in codebase
- Documentation comments added
- All Phase 0 success criteria verified
- **Phase 0 complete - ready for Phase 1 (multi-agent)**
