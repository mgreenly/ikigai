# Task: Create Shared Context Test Helpers

## Target
Phase 0: Shared Context DI - Test Infrastructure

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/tdd.md
- .agents/skills/style.md

## Pre-read Docs
- docs/backlog/shared-context-di.md (test fixture strategy section)

## Pre-read Source (patterns)
- src/shared.h (shared context structure)
- src/shared.c (shared context init)
- src/repl.h (repl context structure)
- tests/helpers/*.h (existing test helpers if any)

## Pre-read Tests (patterns)
- tests/unit/shared/shared_test.c
- tests/unit/repl/*.c (tests that need shared context)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- All Phase 0 field migrations complete
- Multiple test files need to create shared + repl contexts together

## Task
Create test helper functions that simplify creating test contexts. This reduces boilerplate and ensures consistent test setup across the codebase.

The helpers should support:
1. Creating shared_ctx with test defaults (no database, minimal config)
2. Creating shared_ctx + repl_ctx together (most common test need)
3. Creating shared_ctx with custom config (for specific test scenarios)

## TDD Cycle

### Red
1. Create `tests/helpers/test_contexts.h`:
   ```c
   #pragma once

   #include "shared.h"
   #include "repl.h"
   #include "config.h"
   #include "error.h"

   #include <talloc.h>

   // Create minimal config suitable for testing (no database, no API key)
   ik_cfg_t *test_cfg_create(TALLOC_CTX *ctx);

   // Create shared_ctx with test defaults
   res_t test_shared_ctx_create(TALLOC_CTX *ctx, ik_shared_ctx_t **out);

   // Create shared + repl together (most common test need)
   // Both are children of ctx (siblings to each other)
   res_t test_repl_create(TALLOC_CTX *ctx,
                          ik_shared_ctx_t **shared_out,
                          ik_repl_ctx_t **repl_out);

   // Create shared with custom config
   res_t test_shared_ctx_create_with_cfg(TALLOC_CTX *ctx,
                                          ik_cfg_t *cfg,
                                          ik_shared_ctx_t **out);
   ```

2. Create `tests/helpers/test_contexts_test.c`:
   - Test `test_cfg_create()` returns valid config
   - Test `test_shared_ctx_create()` succeeds
   - Test `test_repl_create()` creates both contexts
   - Test cleanup via talloc_free works

3. Run `make check` - expect failures

### Green
1. Create `tests/helpers/test_contexts.c`:
   ```c
   #include "test_contexts.h"

   #include "panic.h"
   #include "wrapper.h"

   ik_cfg_t *test_cfg_create(TALLOC_CTX *ctx)
   {
       ik_cfg_t *cfg = talloc_zero_(ctx, sizeof(ik_cfg_t));
       if (cfg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       // Minimal defaults for testing
       cfg->history_size = 100;
       cfg->db_connection_string = NULL;  // No database in tests by default
       cfg->openai_api_key = NULL;         // No API key in tests
       // ... other required fields with safe defaults

       return cfg;
   }

   res_t test_shared_ctx_create(TALLOC_CTX *ctx, ik_shared_ctx_t **out)
   {
       ik_cfg_t *cfg = test_cfg_create(ctx);
       return ik_shared_ctx_init(ctx, cfg, out);
   }

   res_t test_repl_create(TALLOC_CTX *ctx,
                          ik_shared_ctx_t **shared_out,
                          ik_repl_ctx_t **repl_out)
   {
       ik_shared_ctx_t *shared = NULL;
       res_t result = test_shared_ctx_create(ctx, &shared);
       if (is_err(&result)) return result;

       ik_repl_ctx_t *repl = NULL;
       result = ik_repl_init(ctx, shared, &repl);
       if (is_err(&result)) {
           talloc_free(shared);
           return result;
       }

       *shared_out = shared;
       *repl_out = repl;
       return OK(repl);
   }

   res_t test_shared_ctx_create_with_cfg(TALLOC_CTX *ctx,
                                          ik_cfg_t *cfg,
                                          ik_shared_ctx_t **out)
   {
       return ik_shared_ctx_init(ctx, cfg, out);
   }
   ```

2. Update Makefile to compile test_contexts.c with test builds

3. Run `make check` - expect pass

### Refactor
1. Identify existing tests that create repl_ctx directly
2. Update a few representative tests to use new helpers (as examples)
3. Document helper usage in test_contexts.h
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- Test helper functions exist and work
- At least one existing test file uses new helpers
- Helpers documented for future test authors
- 100% test coverage maintained
- Working tree is clean (all changes committed)
