# Task: Implement ik_repl_restore_agents() loop body

## Target
Gap 1: Startup Agent Restoration Loop (PR2)

## Macro Context

This task is part of **Gap 1: Startup Agent Restoration** - the critical feature that enables multi-agent sessions to persist across restarts.

**What this task accomplishes:**
This task implements the loop body in `ik_repl_restore_agents()` - the complex part that actually restores each agent's complete state. For each running agent from the database, the loop body:

1. **Restores the agent context** - Creates `ik_agent_ctx_t` from DB row via `ik_agent_restore()`
2. **Replays history** - Calls `ik_agent_replay_history()` to get the full message history
3. **Populates conversation** - Filters non-conversation kinds (clear, mark, rewind) and adds messages
4. **Populates scrollback** - Renders each message via `ik_event_render()`
5. **Restores marks** - Copies mark stack from replay context to agent context
6. **Adds to array** - Calls `ik_repl_add_agent()` to add to `repl->agents[]`

**Order of operations matters:**
- Agent restore must happen first (creates the agent context)
- History replay must happen before population (provides the messages)
- Conversation population before scrollback (conversation is canonical state)
- Marks must be restored from replay context's mark_stack
- Adding to array is final step (agent must be fully initialized first)

**Error handling strategy:**
- If an agent fails to restore, log a warning, mark it as 'dead' in DB, and continue
- Don't let one bad agent break the entire startup
- This ensures graceful degradation when data is corrupted

**Relationship to predecessor tasks:**
- `gap1-agent-restore-fn.md` created `ik_agent_restore()` function
- `gap1-restore-agents-skeleton.md` created the skeleton with loop structure
- This task fills in the loop body (replaces the TODO comment)

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
- rel-06/gap.md (Gap 1 section, especially the full `ik_repl_restore_agents()` pseudocode)

## Pre-read Source

**Core files to understand the restoration flow:**
- src/repl/agent_restore.c (READ - skeleton with loop structure to fill in)
- src/repl/agent_restore.h (READ - function declaration)
- src/agent.h (READ - `ik_agent_ctx_t` struct, `ik_agent_restore()` declaration)
- src/agent.c (READ - `ik_agent_restore()` implementation)

**Replay infrastructure:**
- src/db/agent_replay.h (READ - `ik_agent_replay_history()`, `ik_replay_context_t`)
- src/db/replay.h (READ - `ik_replay_context_t`, `ik_replay_mark_stack_t`, `ik_replay_mark_t`)

**Conversation population:**
- src/openai/client.h (READ - `ik_openai_conversation_add_msg()`)
- src/msg.h (READ - `ik_msg_t` structure, kind field)

**Scrollback population:**
- src/event_render.h (READ - `ik_event_render()` for scrollback rendering)
- src/scrollback.h (READ - scrollback structure if needed)

**Agent array management:**
- src/repl.h (READ - `ik_repl_add_agent()`, `ik_repl_ctx_t`)

**Database operations:**
- src/db/agent.h (READ - `ik_db_agent_row_t`, `ik_db_agent_set_status()` for marking dead)

## Source Files to MODIFY
- src/repl/agent_restore.c (MODIFY - implement loop body)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Gap 0 tasks completed (message type unification complete)
- gap1-agent-restore-fn.md completed (`ik_agent_restore()` exists)
- gap1-restore-agents-skeleton.md completed (skeleton with loop structure exists)

## Task

### Part 1: Add required includes

Add these includes to agent_restore.c if not already present:

```c
#include "../db/agent_replay.h"   // ik_agent_replay_history, ik_replay_context_t
#include "../event_render.h"      // ik_event_render
#include "../openai/client.h"     // ik_openai_conversation_add_msg
```

### Part 2: Create helper function to check conversation kinds

Create a helper to filter out non-conversation message kinds:

```c
// Check if a message kind should be included in conversation
// Returns true for user, assistant, system, tool_call, tool_result
// Returns false for clear, mark, rewind (control events, not conversation)
static bool is_conversation_kind(const char *kind)
{
    if (kind == NULL) return false;

    // Control events that don't go in conversation
    if (strcmp(kind, "clear") == 0) return false;
    if (strcmp(kind, "mark") == 0) return false;
    if (strcmp(kind, "rewind") == 0) return false;

    // Everything else is conversation content
    return true;
}
```

### Part 3: Implement the loop body

Replace the TODO placeholder in the loop with the full restoration logic:

```c
for (size_t i = 0; i < count; i++) {
    // Skip Agent 0 (already created in repl_init)
    if (agents[i]->parent_uuid == NULL) {
        continue;
    }

    // --- Step 1: Restore agent context from DB row ---
    ik_agent_ctx_t *agent = NULL;
    res = ik_agent_restore(repl, repl->shared, agents[i], &agent);
    if (is_err(&res)) {
        // Log warning, mark as dead, continue with other agents
        ik_logger_warn(repl->shared->logger,
            "Failed to restore agent %s, marking as dead: %s",
            agents[i]->uuid, res.msg);
        (void)ik_db_agent_set_status(db_ctx, agents[i]->uuid, "dead");
        continue;
    }

    // --- Step 2: Replay history to get message context ---
    ik_replay_context_t *replay_ctx = NULL;
    res = ik_agent_replay_history(db_ctx, agent, agent->uuid, &replay_ctx);
    if (is_err(&res)) {
        ik_logger_warn(repl->shared->logger,
            "Failed to replay history for agent %s, marking as dead: %s",
            agent->uuid, res.msg);
        (void)ik_db_agent_set_status(db_ctx, agent->uuid, "dead");
        talloc_free(agent);
        continue;
    }

    // --- Step 3: Populate conversation (filter non-conversation kinds) ---
    for (size_t j = 0; j < replay_ctx->count; j++) {
        ik_msg_t *msg = replay_ctx->messages[j];
        if (is_conversation_kind(msg->kind)) {
            // Steal message from replay_ctx to agent's conversation
            ik_msg_t *conv_msg = talloc_steal(agent->conversation, msg);
            res = ik_openai_conversation_add_msg(agent->conversation, conv_msg);
            if (is_err(&res)) {
                ik_logger_warn(repl->shared->logger,
                    "Failed to add message to conversation for agent %s: %s",
                    agent->uuid, res.msg);
                // Continue anyway - partial conversation is better than none
            }
        }
    }

    // --- Step 4: Populate scrollback via event render ---
    for (size_t j = 0; j < replay_ctx->count; j++) {
        ik_msg_t *msg = replay_ctx->messages[j];
        res = ik_event_render(agent->scrollback, msg->kind, msg->content, msg->data_json);
        if (is_err(&res)) {
            ik_logger_warn(repl->shared->logger,
                "Failed to render event for agent %s: %s",
                agent->uuid, res.msg);
            // Continue anyway - partial scrollback is better than none
        }
    }

    // --- Step 5: Restore marks from replay context ---
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
                    mark->timestamp = NULL;  // Not preserved in replay
                    agent->marks[agent->mark_count++] = mark;
                }
            }
        }
    }

    // --- Step 6: Add to repl->agents[] array ---
    res = ik_repl_add_agent(repl, agent);
    if (is_err(&res)) {
        ik_logger_warn(repl->shared->logger,
            "Failed to add agent %s to array, marking as dead: %s",
            agent->uuid, res.msg);
        (void)ik_db_agent_set_status(db_ctx, agent->uuid, "dead");
        talloc_free(agent);
        continue;
    }

    ik_logger_debug(repl->shared->logger,
        "Restored agent %s with %zu messages, %zu marks",
        agent->uuid, replay_ctx->count, agent->mark_count);
}
```

### Implementation Notes

1. **Memory ownership:**
   - Messages are talloc_steal'd from replay_ctx to agent's conversation
   - This avoids copying while ensuring proper ownership
   - replay_ctx itself is parented to agent, will be freed when agent is freed

2. **Error handling philosophy:**
   - Each agent restoration is independent
   - If one fails, mark it dead and continue
   - Don't abort the entire startup for one bad agent
   - Use warnings (not errors) since we're recovering gracefully

3. **Mark restoration:**
   - `ik_replay_mark_t.context_idx` maps to `ik_mark_t.message_index`
   - Labels are copied (strdup'd)
   - Timestamps are not preserved in replay (set to NULL)

   **KNOWN LIMITATION:** Mark timestamps are lost after restart. The `ik_mark_t.timestamp` field stores a human-readable timestamp string, but `ik_replay_mark_t` (from replay context) doesn't include this. To preserve timestamps across restarts, the database schema would need a `timestamp` column in the mark event's `data` JSON. This is intentional data loss for now - marks retain their position and label, just not creation time.

4. **Conversation vs Scrollback:**
   - Conversation gets only message kinds (user, assistant, system, tool_call, tool_result)
   - Scrollback gets ALL events (including mark, clear, rewind) via event_render
   - event_render handles clear/mark/rewind appropriately (may clear scrollback, render mark line, etc.)

5. **Logger usage:**
   - Use `ik_logger_warn()` for errors that don't abort
   - Use `ik_logger_debug()` for success logging
   - Access logger via `repl->shared->logger`

## TDD Cycle

### Red
1. Run `make` to verify clean starting state
2. Run `make check` - existing tests should pass
3. Identify what tests are needed for the loop body

### Green
1. Add required includes to agent_restore.c
2. Add `is_conversation_kind()` helper function
3. Implement the loop body replacing the TODO placeholder
4. Run `make` - should compile successfully
5. Run `make check` - all tests should pass
6. Run `make lint` - should pass

### Refactor
1. Review error handling consistency
2. Ensure all edge cases handled (empty replay, no marks, etc.)
3. Verify memory ownership is correct
4. Check for any cleanup needed on error paths

## Unit Tests

Add tests to verify the restoration loop works correctly. Test file: `tests/unit/repl/agent_restore_test.c`

### Test cases to add:

1. **test_restore_single_agent** - restore one non-root agent, verify conversation and scrollback populated
2. **test_restore_skips_agent_zero** - verify root agent (parent_uuid=NULL) is skipped
3. **test_restore_multiple_agents** - restore 2+ agents, verify all in repl->agents[]
4. **test_restore_marks_copied** - verify marks from replay context copied to agent
5. **test_restore_filters_conversation** - verify clear/mark/rewind not in conversation
6. **test_restore_failed_agent_marked_dead** - mock failure, verify agent marked dead in DB
7. **test_restore_continues_after_failure** - verify other agents still restored after one fails

### Test considerations:
- Will need mock/stub for `ik_agent_replay_history()` to return controlled replay context
- Will need mock/stub for database operations
- Consider using existing test infrastructure patterns from other tests

## Sub-agent Usage

This task touches many files and concepts. Use sub-agents for:
- Searching for all usages of `ik_openai_conversation_add_msg()` to understand patterns
- Searching for all usages of `ik_event_render()` to understand patterns
- Running `make` and `make check` for quick feedback
- Searching for existing test patterns for agent/replay tests

## Post-conditions
- `make` compiles successfully
- `make check` passes (all tests including new ones)
- `make lint` passes
- Loop body in `ik_repl_restore_agents()` is fully implemented:
  - Calls `ik_agent_restore()` for each non-root agent
  - Calls `ik_agent_replay_history()` to get message history
  - Populates conversation (filtering non-conversation kinds)
  - Populates scrollback via `ik_event_render()`
  - Restores marks from replay context
  - Calls `ik_repl_add_agent()` to add to array
- Error handling: failed agents logged, marked dead, don't break startup
- Working tree is clean (all changes committed)

## Deviations

If you need to deviate from this plan (e.g., different include paths, different error handling strategy, additional helper functions, changes to mark restoration), document the deviation and reasoning in your final report.
