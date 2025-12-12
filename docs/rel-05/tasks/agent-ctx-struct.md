# Task: Create ik_agent_ctx_t Structure

## Target
Phase 0: Agent Context Extraction - Step 1 (empty struct with UUID identity)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/scm.md

## Pre-read Docs
- docs/agent-process-model.md (agent identity design)
- docs/memory.md (talloc ownership)
- docs/error_handling.md (res_t patterns)

## Pre-read Source (patterns)
- src/shared.h (shared context pattern to follow)
- src/shared.c (init pattern to follow)
- src/repl.h (current ik_repl_ctx_t - fields to extract later)

## Pre-read Tests (patterns)
- tests/unit/shared/shared_test.c (context test patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- No `ik_agent_ctx_t` exists yet

## Task
Create the `ik_agent_ctx_t` structure with UUID-based identity. This establishes the agent context pattern for the process model without migrating state yet.

The agent context will be a **child** of repl_ctx in the talloc hierarchy:

```
root_ctx
  |-> shared_ctx (infrastructure)
  +-> repl_ctx (coordinator)
           +-> agent_ctx (per-agent state)
```

Identity fields (from agent-process-model.md):
- `uuid` - internal unique identifier (for now, generated locally; later from database)
- `name` - optional human-friendly name (NULL initially)
- `parent_uuid` - parent agent's UUID (NULL for root agent)

For Phase 0, we create a single agent with a locally-generated UUID. Database-backed UUIDs come in Phase 1.

## TDD Cycle

### Red
1. Create `src/agent.h`:
   ```c
   #pragma once

   #include "error.h"

   #include <talloc.h>
   #include <stdbool.h>

   // Forward declaration
   typedef struct ik_shared_ctx ik_shared_ctx_t;

   // Per-agent context - state specific to one agent
   // Created as child of repl_ctx (owned by coordinator)
   typedef struct ik_agent_ctx {
       // Identity (from agent-process-model.md)
       char *uuid;          // Internal unique identifier
       char *name;          // Optional human-friendly name (NULL if unnamed)
       char *parent_uuid;   // Parent agent's UUID (NULL for root agent)

       // Reference to shared infrastructure
       ik_shared_ctx_t *shared;

       // Fields will be migrated here incrementally
   } ik_agent_ctx_t;

   // Create agent context
   // ctx: talloc parent (repl_ctx)
   // shared: shared infrastructure
   // parent_uuid: parent agent's UUID (NULL for root agent)
   // out: receives allocated agent context
   res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                         const char *parent_uuid, ik_agent_ctx_t **out);

   // Generate a new UUID as base64url string (helper function)
   // ctx: talloc parent for the returned string
   // Returns: newly allocated 22-character base64url UUID string
   char *ik_agent_generate_uuid(TALLOC_CTX *ctx);
   ```

2. Create `tests/unit/agent/agent_test.c`:
   - Test `ik_agent_create()` succeeds
   - Test `agent->uuid` is non-NULL and exactly 22 characters
   - Test `agent->uuid` contains only base64url characters: [A-Za-z0-9_-]
   - Test `agent->name` is NULL initially
   - Test `agent->parent_uuid` is NULL for root agent
   - Test `agent->parent_uuid` matches input when provided
   - Test `agent->shared` matches input
   - Test agent_ctx is allocated under provided parent
   - Test agent_ctx can be freed via talloc_free
   - Test `ik_agent_generate_uuid()` returns valid 22-char base64url string

3. Run `make check` - expect test failures (implementation missing)

### Green
1. Create `src/agent.c`:
   ```c
   #include "agent.h"

   #include "panic.h"
   #include "wrapper.h"

   #include <assert.h>
   #include <stdio.h>
   #include <stdlib.h>
   #include <time.h>

   // Base64url alphabet (RFC 4648 section 5)
   static const char BASE64URL[] =
       "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

   char *ik_agent_generate_uuid(TALLOC_CTX *ctx)
   {
       assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

       // Generate 16 random bytes (128-bit UUID v4)
       unsigned char bytes[16];
       for (int i = 0; i < 16; i++) {
           bytes[i] = (unsigned char)(rand() & 0xFF);
       }

       // Set version (4) and variant (RFC 4122)
       bytes[6] = (bytes[6] & 0x0F) | 0x40;  // Version 4
       bytes[8] = (bytes[8] & 0x3F) | 0x80;  // Variant 1

       // Encode 16 bytes to 22 base64url characters (no padding)
       // 16 bytes = 128 bits, base64 encodes 6 bits per char
       // ceil(128/6) = 22 characters
       char *uuid = talloc_array(ctx, char, 23);  // 22 chars + null
       if (uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       int j = 0;
       for (int i = 0; i < 16; i += 3) {
           uint32_t n = ((uint32_t)bytes[i] << 16);
           if (i + 1 < 16) n |= ((uint32_t)bytes[i + 1] << 8);
           if (i + 2 < 16) n |= bytes[i + 2];

           uuid[j++] = BASE64URL[(n >> 18) & 0x3F];
           uuid[j++] = BASE64URL[(n >> 12) & 0x3F];
           if (i + 1 < 16) uuid[j++] = BASE64URL[(n >> 6) & 0x3F];
           if (i + 2 < 16) uuid[j++] = BASE64URL[n & 0x3F];
       }
       uuid[j] = '\0';

       return uuid;
   }

   res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                         const char *parent_uuid, ik_agent_ctx_t **out)
   {
       assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
       assert(shared != NULL); // LCOV_EXCL_BR_LINE
       assert(out != NULL);    // LCOV_EXCL_BR_LINE

       ik_agent_ctx_t *agent = talloc_zero_(ctx, sizeof(ik_agent_ctx_t));
       if (agent == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       agent->uuid = ik_agent_generate_uuid(agent);
       agent->name = NULL;  // Unnamed by default
       agent->parent_uuid = parent_uuid ? talloc_strdup(agent, parent_uuid) : NULL;
       agent->shared = shared;

       *out = agent;
       return OK(agent);
   }
   ```

2. Update Makefile to compile agent.c and agent_test.c

3. Run `make check` - expect pass

### Refactor
1. Verify talloc ownership is correct (agent is child of provided parent)
2. Verify uuid/parent_uuid strings are owned by agent (freed with agent)
3. Seed random number generator in test setup for reproducible UUIDs
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `ik_agent_ctx_t` struct exists with UUID identity fields
- `ik_agent_create()` allocates and initializes agent context
- `ik_agent_generate_uuid()` creates valid UUID strings
- Test file exists with identity and creation tests
- No changes to repl_ctx yet (agent not integrated)
- Working tree is clean (all changes committed)
