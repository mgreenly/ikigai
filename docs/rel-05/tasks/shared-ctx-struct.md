# Task: Create ik_shared_ctx_t Structure

## Target
Phase 0: Shared Context DI - Step 1 (Empty struct infrastructure)

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/patterns/context-struct.md
- .agents/skills/patterns/facade.md

### Pre-read Docs
- docs/backlog/shared-context-di.md (design document)
- docs/memory.md (talloc ownership)

### Pre-read Source (patterns)
- src/repl.h (current ik_repl_ctx_t - see fields to migrate)
- src/repl_init.c (current initialization flow)
- src/client.c (main entry point)

### Pre-read Tests (patterns)
- tests/unit/repl/*.c (existing repl test patterns)

## Pre-conditions
- `make check` passes
- No `ik_shared_ctx_t` exists yet

## Task
Create the `ik_shared_ctx_t` structure and `ik_shared_ctx_init()` function as empty infrastructure. This establishes the DI pattern without migrating any fields yet.

The shared context will be a **sibling** of repl_ctx under root_ctx (not a child of repl_ctx). This matches DI principles: create dependencies first, inject into consumers.

## TDD Cycle

### Red
1. Create `src/shared.h`:
   ```c
   #pragma once

   #include "error.h"

   #include <talloc.h>

   // Shared infrastructure context - resources shared across all agents
   // Created as sibling to repl_ctx under root_ctx (DI pattern)
   typedef struct ik_shared_ctx {
       // Fields will be migrated here incrementally
       // Currently empty - infrastructure only
   } ik_shared_ctx_t;

   // Create shared context (facade that will create infrastructure)
   // ctx: talloc parent (root_ctx)
   // out: receives allocated shared context
   res_t ik_shared_ctx_init(TALLOC_CTX *ctx, ik_shared_ctx_t **out);
   ```

2. Create `tests/unit/shared/shared_test.c`:
   - Test `ik_shared_ctx_init()` succeeds
   - Test shared_ctx is allocated under provided parent
   - Test shared_ctx can be freed via talloc_free

3. Run `make check` - expect test failures (implementation missing)

### Green
1. Create `src/shared.c`:
   ```c
   #include "shared.h"

   #include "panic.h"
   #include "wrapper.h"

   #include <assert.h>

   res_t ik_shared_ctx_init(TALLOC_CTX *ctx, ik_shared_ctx_t **out)
   {
       assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
       assert(out != NULL);   // LCOV_EXCL_BR_LINE

       ik_shared_ctx_t *shared = talloc_zero_(ctx, sizeof(ik_shared_ctx_t));
       if (shared == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       *out = shared;
       return OK(shared);
   }
   ```

2. Update Makefile to compile shared.c and shared_test.c

3. Run `make check` - expect pass

### Refactor
1. Verify talloc ownership is correct (shared_ctx is child of provided parent)
2. Verify no memory leaks with valgrind/sanitizers
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `ik_shared_ctx_t` struct exists (empty)
- `ik_shared_ctx_init()` allocates and returns shared context
- Test file exists with basic creation tests
- No changes to existing repl initialization yet
