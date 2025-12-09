# Task: Create ik_agent_ctx_t Structure

## Target
Phase 1: Agent Context Extraction - Step 1 (Empty struct with identity)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/patterns/context-struct.md
- .agents/skills/patterns/factory.md

## Pre-read Docs
- docs/backlog/shared-context-di.md (design document)
- docs/rel-05/scratch.md (Phase 1 section, lines 130-193)
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/shared.h (shared context pattern to follow)
- src/shared.c (init pattern to follow)
- src/repl.h (current ik_repl_ctx_t - fields to extract)
- src/repl_init.c (current initialization flow)

## Pre-read Tests (patterns)
- tests/unit/shared/shared_test.c (context test patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Phase 0 complete (ik_shared_ctx_t exists with all shared fields)
- No `ik_agent_ctx_t` exists yet

## Task
Create the `ik_agent_ctx_t` structure with identity fields only. This establishes the agent context pattern without migrating state yet.

The agent context will be a **child** of repl_ctx in the talloc hierarchy:

```
root_ctx
  ├─> shared_ctx (infrastructure)
  └─> repl_ctx (coordinator)
           └─> agent (per-agent state)
```

Identity fields:
- `agent_id` - string like "0/", "1/", etc.
- `numeric_id` - just the number part (0, 1, 2, ...)

For now, repl_ctx will hold a single agent. Multi-agent array comes later (manual-top-level-agents work).

## TDD Cycle

### Red
1. Create `src/agent.h`:
   ```c
   #pragma once

   #include "error.h"

   #include <talloc.h>
   #include <inttypes.h>

   // Per-agent context - state specific to one agent
   // Created as child of repl_ctx (owned by coordinator)
   typedef struct ik_agent_ctx {
       // Identity
       char *agent_id;      // "0/", "1/", etc.
       size_t numeric_id;   // Just the number (0, 1, 2, ...)

       // Fields will be migrated here incrementally
   } ik_agent_ctx_t;

   // Create agent context
   // ctx: talloc parent (repl_ctx)
   // numeric_id: agent number (0 for initial agent)
   // out: receives allocated agent context
   res_t ik_agent_create(TALLOC_CTX *ctx, size_t numeric_id, ik_agent_ctx_t **out);
   ```

2. Create `tests/unit/agent/agent_test.c`:
   - Test `ik_agent_create()` succeeds
   - Test agent_ctx is allocated under provided parent
   - Test `agent->numeric_id` matches input
   - Test `agent->agent_id` is correctly formatted ("0/", "1/", etc.)
   - Test agent_ctx can be freed via talloc_free

3. Run `make check` - expect test failures (implementation missing)

### Green
1. Create `src/agent.c`:
   ```c
   #include "agent.h"

   #include "panic.h"
   #include "wrapper.h"

   #include <assert.h>
   #include <stdio.h>

   res_t ik_agent_create(TALLOC_CTX *ctx, size_t numeric_id, ik_agent_ctx_t **out)
   {
       assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
       assert(out != NULL);  // LCOV_EXCL_BR_LINE

       ik_agent_ctx_t *agent = talloc_zero_(ctx, sizeof(ik_agent_ctx_t));
       if (agent == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       agent->numeric_id = numeric_id;

       // Format agent_id as "N/"
       agent->agent_id = talloc_asprintf(agent, "%zu/", numeric_id);
       if (agent->agent_id == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       *out = agent;
       return OK(agent);
   }
   ```

2. Update Makefile to compile agent.c and agent_test.c

3. Run `make check` - expect pass

### Refactor
1. Verify talloc ownership is correct (agent is child of provided parent)
2. Verify agent_id string is owned by agent (freed with agent)
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `ik_agent_ctx_t` struct exists with identity fields
- `ik_agent_create()` allocates and initializes agent context
- Test file exists with basic creation tests
- No changes to repl_ctx yet (agent not integrated)
- Working tree is clean (all changes committed)
