# Task: Implement ik_repl_restore_agents() skeleton

## Target
Gap 1: Startup Agent Restoration Loop (PR2)

## Macro Context

This task is part of **Gap 1: Startup Agent Restoration** - the critical feature that enables multi-agent sessions to persist across restarts.

**What this task accomplishes:**
- Creates the skeleton of `ik_repl_restore_agents()` function
- Queries all running agents from database using `ik_db_agent_list_running()`
- Sorts agents by `created_at` (oldest first)
- Sets up the loop structure (body implemented in next task)

**Why sorting by created_at matters:**
- Parents are always created before children (fork creates child after parent exists)
- Sorting oldest-first guarantees parent agents are restored before their children
- This ensures `parent_uuid` references point to agents that already exist in memory
- Without sorting, a child might be restored before its parent, breaking hierarchy

**What this task does NOT do (next task):**
- Loop body: calling `ik_agent_restore()` for each agent
- History replay: calling `ik_agent_replay_history()`
- Conversation/scrollback population

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
- rel-06/gap.md (Gap 1 section, especially the "Proposed Fix" pseudocode for `ik_repl_restore_agents()`)

## Pre-read Source
- src/repl_init.c (READ - current initialization flow, understand where restore will be called)
- src/repl.h (READ - `ik_repl_ctx_t` struct, understand repl context)
- src/db/agent.h (READ - `ik_db_agent_list_running()` and `ik_db_agent_row_t`)
- src/db/agent.c (READ - implementation of `ik_db_agent_list_running()`)
- src/error.h (READ - CHECK macro, res_t type)

## Source Files to CREATE
- src/repl/agent_restore.h (CREATE - header with `ik_repl_restore_agents()` declaration)
- src/repl/agent_restore.c (CREATE - skeleton implementation)

## Source Files to MODIFY
None in this task - we're creating new files only.

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Gap 0 tasks completed (message type unification complete)
- gap1-agent-restore-fn.md completed (`ik_agent_restore()` exists in src/agent.h and src/agent.c)

## Task

### Part 1: Create src/repl/agent_restore.h

Create the header file with the function declaration:

```c
#ifndef IK_REPL_AGENT_RESTORE_H
#define IK_REPL_AGENT_RESTORE_H

#include "../error.h"
#include "../repl.h"
#include "../db/connection.h"

/**
 * Restore all running agents from database on startup
 *
 * Queries all agents with status='running', sorts by created_at (oldest first),
 * and restores each one (replay history, populate conversation/scrollback).
 *
 * Sorting by created_at ensures parents are restored before children, since
 * a parent must exist before a child can be forked from it.
 *
 * Agent 0 (root agent with parent_uuid=NULL) is skipped because it is already
 * created during ik_repl_init() before this function is called.
 *
 * @param repl REPL context (must not be NULL, must have current agent set)
 * @param db_ctx Database context (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_repl_restore_agents(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx);

#endif // IK_REPL_AGENT_RESTORE_H
```

### Part 2: Create src/repl/agent_restore.c

Create the skeleton implementation:

```c
// Agent restoration on startup
#include "agent_restore.h"

#include "../db/agent.h"
#include "../error.h"
#include "../logger.h"
#include "../repl.h"

#include <assert.h>
#include <stdlib.h>
#include <talloc.h>

// Comparison function for qsort - sort by created_at ascending (oldest first)
static int compare_agents_by_created_at(const void *a, const void *b)
{
    const ik_db_agent_row_t *agent_a = *(const ik_db_agent_row_t **)a;
    const ik_db_agent_row_t *agent_b = *(const ik_db_agent_row_t **)b;

    // Sort ascending: older agents first (lower created_at values)
    if (agent_a->created_at < agent_b->created_at) return -1;
    if (agent_a->created_at > agent_b->created_at) return 1;
    return 0;
}

res_t ik_repl_restore_agents(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx)
{
    assert(repl != NULL);       // LCOV_EXCL_BR_LINE
    assert(db_ctx != NULL);     // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) {  // LCOV_EXCL_BR_LINE
        return ERR(repl, "Out of memory");  // LCOV_EXCL_LINE
    }

    // 1. Query all running agents from database
    ik_db_agent_row_t **agents = NULL;
    size_t count = 0;
    res_t res = ik_db_agent_list_running(db_ctx, tmp, &agents, &count);
    if (is_err(&res)) {
        talloc_free(tmp);
        return res;
    }

    // 2. Sort by created_at (oldest first) - parents before children
    if (count > 1) {
        qsort(agents, count, sizeof(ik_db_agent_row_t *), compare_agents_by_created_at);
    }

    // 3. For each agent (loop structure only, body in next task):
    for (size_t i = 0; i < count; i++) {
        // Skip Agent 0 (already created in repl_init)
        // Agent 0 is the root agent with no parent
        if (agents[i]->parent_uuid == NULL) {
            continue;
        }

        // TODO: Loop body in next task (gap1-restore-agents-loop.md)
        // - Call ik_agent_restore() to create agent context from DB row
        // - Call ik_agent_replay_history() to get message history
        // - Populate conversation from replay context
        // - Populate scrollback from replay context
        // - Restore marks from replay context
        // - Add agent to repl->agents[] array
        (void)agents[i];  // Suppress unused warning until loop body implemented
    }

    talloc_free(tmp);
    return OK(NULL);
}
```

### Part 3: Update Makefile

Verify that the new source file is picked up by the Makefile. The project uses a glob pattern for source files, so no Makefile changes should be needed. Confirm by running `make`.

### Implementation Notes

1. **qsort comparison function:**
   - Must return negative if a < b, positive if a > b, zero if equal
   - We dereference twice: `void *` -> `ik_db_agent_row_t **` -> `ik_db_agent_row_t *`
   - Ascending sort (oldest first) means smaller `created_at` values come first

2. **Agent 0 detection:**
   - Agent 0 is the root agent with `parent_uuid == NULL`
   - It's created during `ik_repl_init()` before `ik_repl_restore_agents()` is called
   - We skip it to avoid duplicate restoration

3. **Memory management:**
   - Use temporary talloc context for DB query results
   - Query results are owned by `tmp`, freed at end of function
   - Agent contexts created in loop will be parented to `repl` (in next task)

4. **Assertions:**
   - Both parameters are required (not NULL)
   - Use standard LCOV exclusion for assertion branches

## TDD Cycle

### Red
1. Run `make` to verify clean starting state
2. Create header file with declaration
3. Run `make` - should fail (function declared in header but no source file yet)

### Green
1. Create source file with skeleton implementation
2. Run `make` - should compile successfully
3. Run `make check` - all existing tests should pass
4. Run `make lint` - should pass

### Refactor
1. Review for style consistency with existing code
2. Ensure assertions follow project pattern
3. Verify memory management is correct

## Unit Tests

No new unit tests in this task - the function is a skeleton with an empty loop body.

Tests will be added in the next task (gap1-restore-agents-loop.md) when the loop body is implemented.

However, verify the function compiles and can be called by:
1. Checking `make` succeeds
2. Checking `make check` passes (no regressions)

## Sub-agent Usage
- Use haiku sub-agents to run `make` and `make check` for quick feedback
- If you need to verify the qsort comparison function, use a sub-agent

## Post-conditions
- `make` compiles successfully
- `make check` passes (all existing tests)
- `make lint` passes
- src/repl/agent_restore.h exists with `ik_repl_restore_agents()` declaration
- src/repl/agent_restore.c exists with skeleton implementation:
  - Queries all running agents from DB
  - Sorts by created_at (oldest first)
  - Loop structure with Agent 0 skip logic
  - Loop body is TODO placeholder
- Working tree is clean (all changes committed)

## Deviations

If you need to deviate from this plan (e.g., different include paths, additional error handling, different file locations), document the deviation and reasoning in your final report.
