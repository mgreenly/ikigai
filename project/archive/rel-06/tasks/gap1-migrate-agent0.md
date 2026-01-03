# Task: Migrate Agent 0 to agent-based replay

## Target
Gap 1 + Gap 5 merged: Startup Agent Restoration Loop (PR2)

## Macro Context

This task merges **Gap 5** (Session Restore Migration) into **Gap 1** (Agent Restoration). The key insight from Gap 5 in gap.md:

> Merge Gap 5 into Gap 1 - restore ALL agents (including Agent 0) using the same `ik_repl_restore_agents()` function.

**Why this matters:**
Agent 0 (the root agent) should use the same restoration path as all other agents. This eliminates a special case and removes the legacy session-based replay system.

**Before (current state):**
- session_restore.c uses `ik_db_messages_load()` (session-based)
- Agent 0 is handled separately from other agents
- Two different code paths for restoration

**After (target state):**
- Agent 0 goes through `ik_repl_restore_agents()` using `ik_agent_replay_history()` (agent-based)
- Single unified restoration path for ALL agents
- Legacy `ik_repl_restore_session_()` call removed from repl_init.c
- session_restore.c becomes obsolete (deletion deferred to cleanup task)

**The key difference for Agent 0:**
Agent 0's context already exists (`repl->current`) when `ik_repl_restore_agents()` runs. For other agents, we create new contexts. For Agent 0, we use the existing context and just populate its conversation/scrollback/marks.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/source-code.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- rel-06/gap.md (Gap 1 AND Gap 5 sections)

## Pre-read Source

**Core files to understand the current restoration flow:**
- src/repl_init.c (READ - current initialization flow with separate session restore)
- src/repl/session_restore.c (READ - legacy session-based restoration to replace)
- src/repl/session_restore.h (READ - function declaration)

**Agent restoration infrastructure:**
- src/repl/agent_restore.c (READ - agent restoration loop to modify)
- src/repl/agent_restore.h (READ - function declaration)
- src/db/agent_replay.h (READ - `ik_agent_replay_history()`)
- src/db/replay.h (READ - `ik_replay_context_t`, `ik_replay_mark_t`)

**Supporting context:**
- src/repl.h (READ - `ik_repl_ctx_t`, `repl->current`)
- src/agent.h (READ - `ik_agent_ctx_t` structure)
- src/openai/client.h (READ - `ik_openai_conversation_add_msg()`)
- src/event_render.h (READ - `ik_event_render()`)

## Source Files to MODIFY
- src/repl/agent_restore.c (MODIFY - remove "skip Agent 0" logic, add Agent 0 handling)
- src/repl_init.c (MODIFY - remove `ik_repl_restore_session_()` call)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Gap 0 tasks completed (message type unification)
- gap1-agent-restore-fn.md completed
- gap1-restore-agents-skeleton.md completed
- gap1-restore-loop-body.md completed
- gap1-integrate-repl-init.md completed (`ik_repl_restore_agents()` is called in repl_init)

## Task

### Part 1: Modify ik_repl_restore_agents() to handle Agent 0

In `src/repl/agent_restore.c`, modify the loop to handle Agent 0 specially:

**Current behavior (to change):**
```c
for (size_t i = 0; i < count; i++) {
    // Skip Agent 0 (already created in repl_init)
    if (agents[i]->parent_uuid == NULL) {
        continue;
    }
    // ... restore other agents
}
```

**New behavior:**
```c
for (size_t i = 0; i < count; i++) {
    if (agents[i]->parent_uuid == NULL) {
        // This is Agent 0 - use existing repl->current, don't create new
        ik_agent_ctx_t *agent = repl->current;

        // Replay history
        ik_replay_context_t *replay_ctx = NULL;
        res = ik_agent_replay_history(db_ctx, tmp, agents[i]->uuid, &replay_ctx);
        if (is_err(&res)) {
            // For Agent 0, failure is more serious - log but continue
            // (Agent 0 can still function with empty state)
            ik_logger_warn(repl->shared->logger,
                "Failed to replay history for Agent 0: %s", res.msg);
            continue;
        }

        // Populate conversation (filter non-conversation kinds)
        for (size_t j = 0; j < replay_ctx->count; j++) {
            ik_msg_t *msg = replay_ctx->messages[j];
            if (is_conversation_kind(msg->kind)) {
                ik_msg_t *conv_msg = talloc_steal(agent->conversation, msg);
                res = ik_openai_conversation_add_msg(agent->conversation, conv_msg);
                if (is_err(&res)) {
                    ik_logger_warn(repl->shared->logger,
                        "Failed to add message to Agent 0 conversation: %s", res.msg);
                }
            }
        }

        // Populate scrollback via event render
        for (size_t j = 0; j < replay_ctx->count; j++) {
            ik_msg_t *msg = replay_ctx->messages[j];
            res = ik_event_render(agent->scrollback, msg->kind, msg->content, msg->data_json);
            if (is_err(&res)) {
                ik_logger_warn(repl->shared->logger,
                    "Failed to render event for Agent 0: %s", res.msg);
            }
        }

        // Restore marks from replay context
        if (replay_ctx->mark_stack.count > 0) {
            agent->marks = talloc_array(agent, ik_mark_t *, replay_ctx->mark_stack.count);
            if (agent->marks != NULL) {
                for (size_t j = 0; j < replay_ctx->mark_stack.count; j++) {
                    ik_replay_mark_t *replay_mark = &replay_ctx->mark_stack.marks[j];
                    ik_mark_t *mark = talloc_zero(agent, ik_mark_t);
                    if (mark != NULL) {
                        mark->message_index = replay_mark->context_idx;
                        mark->label = replay_mark->label
                            ? talloc_strdup(mark, replay_mark->label)
                            : NULL;
                        mark->timestamp = NULL;
                        agent->marks[agent->mark_count++] = mark;
                    }
                }
            }
        }

        // Don't add to array - Agent 0 is already in repl->agents[]
        ik_logger_debug(repl->shared->logger,
            "Restored Agent 0 with %zu messages, %zu marks",
            replay_ctx->count, agent->mark_count);

        continue;  // Skip the regular agent creation path
    }

    // ... rest of loop for non-root agents (unchanged)
}
```

### Part 2: Remove legacy session restore from repl_init.c

In `src/repl_init.c`, remove the `ik_repl_restore_session_()` call:

**Current code (to remove):**
```c
// Restore session if database is configured (must be after repl allocated)
if (shared->db_ctx != NULL) {
    result = ik_repl_restore_session_(repl, shared->db_ctx, cfg);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }
}
```

**After removal:**
The `ik_repl_restore_agents()` call (added by gap1-integrate-repl-init.md) now handles Agent 0's restoration.

### Part 3: Remove session_restore.h include from repl_init.c

Remove the include that's no longer needed:
```c
// Remove this line:
#include "repl/session_restore.h"
```

### Implementation Notes

1. **Agent 0 exists but is empty:** When `ik_repl_restore_agents()` runs, Agent 0 (`repl->current`) already exists in `repl->agents[0]` but its conversation/scrollback/marks are empty. We populate them from replay.

2. **No new context creation:** For Agent 0, we use `repl->current` directly. For other agents, we call `ik_agent_restore()` which creates a new context.

3. **Error handling difference:** For Agent 0, we log but continue (Agent 0 must exist for ikigai to function). For other agents, we can mark them dead and skip.

4. **Fresh install case:** If Agent 0 has no history (new installation), `ik_agent_replay_history()` returns an empty replay context, which is fine - Agent 0 starts with empty state. The initial clear/system events will be created when the user first interacts.

5. **Memory ownership:** Messages are talloc_steal'd from replay_ctx to agent's conversation, same as for other agents.

6. **session_restore.c not deleted yet:** We don't delete session_restore.c in this task - it becomes dead code. A separate cleanup task will remove it along with `ik_db_messages_load()`.

## TDD Cycle

### Red
1. Run `make` to verify clean starting state
2. Run `make check` - all tests should pass
3. Write a test that verifies Agent 0 is restored via agent_restore, not session_restore

### Green
1. Modify the loop in agent_restore.c to handle Agent 0
2. Remove the `ik_repl_restore_session_()` call from repl_init.c
3. Remove the `#include "repl/session_restore.h"` from repl_init.c
4. Run `make` - should compile successfully
5. Run `make check` - all tests should pass
6. Run `make lint` - should pass

### Refactor
1. Ensure the Agent 0 handling is consistent with other agent handling
2. Consider extracting the common restoration logic (populate conversation, scrollback, marks) into a helper function
3. Verify error handling is appropriate for Agent 0 vs other agents

## Unit Tests

Modify `tests/unit/repl/agent_restore_test.c` to add:

### Test cases to add:

1. **test_restore_agent_zero** - verify Agent 0 (parent_uuid=NULL) has its conversation/scrollback populated
2. **test_restore_agent_zero_uses_existing_context** - verify repl->current is used, not a new context
3. **test_restore_agent_zero_empty_history** - verify fresh install case (empty replay) works
4. **test_restore_all_agents_including_zero** - verify Agent 0 AND other agents are all restored

### Tests to modify:

1. **test_restore_skips_agent_zero** - either remove or rename to test_restore_agent_zero_uses_existing_context

## Sub-agent Usage

This is a significant refactor. Use sub-agents for:
- Searching for all usages of `ik_repl_restore_session_()` to ensure no other callers
- Running `make` and `make check` for quick feedback
- Searching for existing Agent 0 handling patterns in tests
- Verifying session_restore.c becomes dead code (no remaining callers)

## Post-conditions
- `make` compiles successfully
- `make check` passes (all tests including new ones)
- `make lint` passes
- `ik_repl_restore_agents()` handles Agent 0 (populates existing context)
- `ik_repl_restore_session_()` is NOT called from repl_init.c
- Agent 0's conversation, scrollback, and marks are populated via `ik_agent_replay_history()`
- Other agents still restored correctly via the same loop
- session_restore.c is dead code (no remaining callers in repl_init.c)
- Working tree is clean (all changes committed)

## Deviations

If you need to deviate from this plan (e.g., different approach to Agent 0 handling, refactoring the common logic into a helper, additional error handling), document the deviation and reasoning in your final report.
