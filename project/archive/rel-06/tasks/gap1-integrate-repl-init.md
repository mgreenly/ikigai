# Task: Integrate ik_repl_restore_agents() into ik_repl_init()

## Target
Gap 1: Startup Agent Restoration Loop (PR2)

## Macro Context

This task is part of **Gap 1: Startup Agent Restoration** - the critical feature that enables multi-agent sessions to persist across restarts.

**What this task accomplishes:**
This task wires the new `ik_repl_restore_agents()` function into the initialization flow in `ik_repl_init()`. This is the integration point that actually triggers agent restoration at startup.

**Where in the init flow this goes:**
The call to `ik_repl_restore_agents()` must be placed:
1. AFTER Agent 0 registration (line ~111 in repl_init.c) - Agent 0 must exist first
2. BEFORE `ik_repl_restore_session_()` call - other agents should be restored before session restore

**Current init flow (lines 102-120):**
```c
// Ensure Agent 0 exists in registry if database is configured
if (shared->db_ctx != NULL) {
    char *agent_zero_uuid = NULL;
    result = ik_db_ensure_agent_zero(shared->db_ctx, &agent_zero_uuid);
    // ... Agent 0 setup ...
    repl->current->uuid = talloc_steal(repl->current, agent_zero_uuid);
}

// Restore session if database is configured
if (shared->db_ctx != NULL) {
    result = ik_repl_restore_session_(repl, shared->db_ctx, cfg);
    // ...
}
```

**Target init flow after this task:**
```c
// Ensure Agent 0 exists in registry if database is configured
if (shared->db_ctx != NULL) {
    char *agent_zero_uuid = NULL;
    result = ik_db_ensure_agent_zero(shared->db_ctx, &agent_zero_uuid);
    // ... Agent 0 setup ...
    repl->current->uuid = talloc_steal(repl->current, agent_zero_uuid);

    // NEW: Restore all other running agents from database
    result = ik_repl_restore_agents(repl, shared->db_ctx);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }
}

// Keep existing session restore for now (will be migrated in later task)
if (shared->db_ctx != NULL) {
    result = ik_repl_restore_session_(repl, shared->db_ctx, cfg);
    // ...
}
```

**Important notes:**
- We keep the existing `ik_repl_restore_session_()` call for now
- Session restore handles Agent 0's conversation/scrollback restoration
- Agent restore handles all OTHER agents (skips Agent 0 which has parent_uuid=NULL)
- The session restore will be migrated to agent-based replay in a later task (Gap 5)

**Relationship to predecessor tasks:**
- `gap1-agent-restore-fn.md` created `ik_agent_restore()` function
- `gap1-restore-agents-skeleton.md` created `ik_repl_restore_agents()` skeleton
- `gap1-restore-loop-body.md` implemented the loop body
- This task (final step) integrates everything into the startup flow

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/source-code.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- rel-06/gap.md (Gap 1 section)

## Pre-read Source

**Core files to understand the integration point:**
- src/repl_init.c (READ - current initialization flow, the file to modify)
- src/repl/agent_restore.h (READ - function declaration for ik_repl_restore_agents)
- src/repl/agent_restore.c (READ - implementation to understand what the function does)

**Supporting context:**
- src/repl.h (READ - ik_repl_ctx_t structure)
- src/db/agent.h (READ - ik_db_ensure_agent_zero used in init flow)
- src/repl/session_restore.h (READ - existing session restore to understand the flow)

## Source Files to MODIFY
- src/repl_init.c (MODIFY - add include, add ik_repl_restore_agents call)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Gap 0 tasks completed (message type unification complete)
- gap1-agent-restore-fn.md completed (`ik_agent_restore()` exists in agent.c)
- gap1-restore-agents-skeleton.md completed (`ik_repl_restore_agents()` skeleton exists)
- gap1-restore-loop-body.md completed (loop body implemented)

## Task

### Part 1: Add include for agent_restore.h

At the top of `src/repl_init.c`, add the include for the agent restore header after the existing repl includes:

```c
#include "repl.h"

#include "agent.h"
#include "repl/session_restore.h"
#include "repl/agent_restore.h"    // NEW: Add this include
#include "config.h"
// ... rest of includes
```

### Part 2: Add ik_repl_restore_agents() call

Inside `ik_repl_init()`, after the Agent 0 registration block (after line 110 where `repl->current->uuid` is set), add the call to restore other agents:

**Location:** Inside the first `if (shared->db_ctx != NULL)` block, after `repl->current->uuid = talloc_steal(repl->current, agent_zero_uuid);`

**Code to add:**
```c
    // Restore all other running agents from database
    result = ik_repl_restore_agents(repl, shared->db_ctx);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }
```

**Full context of the modified block:**
```c
// Ensure Agent 0 exists in registry if database is configured
if (shared->db_ctx != NULL) {
    char *agent_zero_uuid = NULL;
    result = ik_db_ensure_agent_zero(shared->db_ctx, &agent_zero_uuid);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }
    repl->current->uuid = talloc_steal(repl->current, agent_zero_uuid);

    // Restore all other running agents from database
    result = ik_repl_restore_agents(repl, shared->db_ctx);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }
}

// Restore session if database is configured (must be after repl allocated)
// NOTE: This handles Agent 0's conversation/scrollback. Will be migrated to
// agent-based replay in a later task (Gap 5).
if (shared->db_ctx != NULL) {
    result = ik_repl_restore_session_(repl, shared->db_ctx, cfg);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }
}
```

### Implementation Notes

1. **Error handling:** If `ik_repl_restore_agents()` fails, we abort startup. This is different from the internal behavior (individual agent failures are logged and continued) - if the entire restoration system fails, that's a critical error.

2. **Order matters:**
   - Agent 0 must be registered first (provides root of the agent tree)
   - Other agents are restored next (they reference Agent 0 as ancestor)
   - Session restore remains last (handles Agent 0's content for now)

3. **Why inside the same if block:** The `ik_repl_restore_agents()` call is placed inside the `if (shared->db_ctx != NULL)` block because agent restoration requires database access. Without a database, there are no agents to restore.

4. **Comment update:** Consider adding a clarifying comment to the session restore block indicating it will be migrated later.

## TDD Cycle

### Red
1. Run `make` to verify clean starting state
2. Run `make check` - all tests should pass before changes
3. The actual "red" state is conceptual: currently multi-agent sessions don't persist across restarts

### Green
1. Add the include for `repl/agent_restore.h`
2. Add the `ik_repl_restore_agents()` call in the correct location
3. Run `make` - should compile successfully
4. Run `make check` - all tests should pass
5. Run `make lint` - should pass

### Refactor
1. Ensure error handling is consistent with surrounding code
2. Verify comments are clear about the flow
3. Consider if the two `if (shared->db_ctx != NULL)` blocks should be merged (they intentionally stay separate for clarity and future migration)

## Unit Tests

This is primarily an integration task. The unit tests for `ik_repl_restore_agents()` are covered by the predecessor task (gap1-restore-loop-body.md).

**Integration verification:**
- Manually test by running ikigai with a database containing multiple running agents
- Verify agents are restored on startup
- Verify Agent 0's session is still restored via the existing path

**Existing test coverage:**
- `tests/unit/repl/agent_restore_test.c` - tests the restoration logic itself
- `tests/unit/repl_init_test.c` - may need a test verifying the integration (optional)

## Sub-agent Usage

This is a small, focused task. Sub-agents can be used for:
- Running `make` to verify compilation
- Running `make check` to verify tests pass
- Running `make lint` to verify style compliance

## Post-conditions
- `make` compiles successfully
- `make check` passes (all tests)
- `make lint` passes
- `src/repl_init.c` includes `repl/agent_restore.h`
- `ik_repl_restore_agents()` is called in `ik_repl_init()` after Agent 0 registration
- The call is placed before `ik_repl_restore_session_()`
- Error handling properly aborts init on failure
- Working tree is clean (all changes committed)

## Deviations

If you need to deviate from this plan (e.g., different include path, different placement in the init flow, merging the if blocks), document the deviation and reasoning in your final report.
