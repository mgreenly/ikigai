# Task: Handle Fresh Install in Agent Restoration

## Target
Gap 1 + Gap 5 merged: Startup Agent Restoration Loop (PR2)

## Macro Context

This task handles the **fresh install** case - when Agent 0 has no history in the database and we need to write initial events to establish the conversation.

**What this handles:**
When `ik_agent_replay_history()` returns an empty replay context (count == 0) for Agent 0, it means this is either:
1. A fresh installation (no prior database entries)
2. A new session after a `/clear` command that purged history

**Why this is needed:**
The legacy `session_restore.c` (lines 130-177) handled fresh installs by:
1. Creating a session via `ik_db_session_create()`
2. Writing a clear event via `ik_db_message_insert()`
3. Writing a system message if `cfg->openai_system_message` is configured
4. Adding the system message to scrollback

As we remove `session_restore.c` (Gap 5 merged into Gap 1), this logic must be preserved in the new agent-based restoration path.

**What happens on fresh install:**
1. Write initial "clear" event to database (establishes session start marker)
2. If `cfg->openai_system_message` is configured:
   - Write "system" event to database
   - Add system message to Agent 0's scrollback via `ik_event_render()`
   - Add system message to Agent 0's conversation for LLM context

**Where to implement (Option A - recommended):**
Handle in `ik_repl_restore_agents()` after Agent 0 restoration completes. Check if Agent 0's replay context has count == 0, and if so, write initial events. This keeps the logic encapsulated within the agent restoration flow.

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

**Legacy fresh install handling (to replicate):**
- src/repl/session_restore.c (READ lines 130-177 - the "new session" path)
- src/repl/session_restore.h (READ - function declaration)

**Agent restoration infrastructure (to modify):**
- src/repl/agent_restore.c (READ - if exists after predecessor tasks)
- src/repl/agent_restore.h (READ - if exists after predecessor tasks)

**Database and rendering APIs:**
- src/db/message.h (READ - `ik_db_message_insert()` for writing events)
- src/event_render.h (READ - `ik_event_render()` for scrollback rendering)
- src/openai/client.h (READ - `ik_openai_conversation_add_msg()` for LLM context)

**Configuration access:**
- src/config.h (READ - `openai_system_message` field in `ik_cfg_t`)
- src/repl.h (READ - `ik_repl_ctx_t`, access to config via `repl->shared->cfg`)
- src/shared.h (READ - `ik_shared_ctx_t` for `cfg` and `session_id`)

## Source Files to MODIFY
- src/repl/agent_restore.c (MODIFY - add fresh install handling after Agent 0 restoration)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- gap1-migrate-agent0.md completed (Agent 0 is restored via `ik_repl_restore_agents()`)
- Agent 0's replay context is available after restoration

## Task

### Part 1: Understand the fresh install detection

After Agent 0 is processed in `ik_repl_restore_agents()`, check if it has no history:

```c
// After Agent 0 restoration, check if this is a fresh install
if (agent_0_replay_ctx->count == 0) {
    // Fresh install - Agent 0 has no history
    // Need to write initial clear event and optional system message
}
```

### Part 2: Implement fresh install handling in agent_restore.c

Add this logic after Agent 0's restoration in `ik_repl_restore_agents()`:

```c
// After Agent 0 restoration completes, check for fresh install
if (replay_ctx->count == 0) {
    // Fresh install - write initial events for Agent 0

    // 1. Write clear event to establish session start
    res_t clear_res = ik_db_message_insert(
        db_ctx,
        repl->shared->session_id,
        repl->current->uuid,
        "clear",
        NULL,
        "{}"
    );
    if (is_err(&clear_res)) {
        ik_logger_warn(repl->shared->logger,
            "Failed to write initial clear event: %s", clear_res.msg);
        // Continue anyway - not fatal for fresh install
    }

    // 2. Write system message if configured
    ik_cfg_t *cfg = repl->shared->cfg;
    if (cfg != NULL && cfg->openai_system_message != NULL) {
        // Write to database
        res_t system_res = ik_db_message_insert(
            db_ctx,
            repl->shared->session_id,
            repl->current->uuid,
            "system",
            cfg->openai_system_message,
            "{}"
        );
        if (is_err(&system_res)) {
            ik_logger_warn(repl->shared->logger,
                "Failed to write system message: %s", system_res.msg);
        } else {
            // Add to scrollback for display
            res_t render_res = ik_event_render(
                repl->current->scrollback,
                "system",
                cfg->openai_system_message,
                "{}"
            );
            if (is_err(&render_res)) {
                ik_logger_warn(repl->shared->logger,
                    "Failed to render system message: %s", render_res.msg);
            }

            // Add to conversation for LLM context
            ik_msg_t *sys_msg = ik_msg_create(repl->current->conversation,
                                               "system",
                                               cfg->openai_system_message,
                                               "{}");
            if (sys_msg != NULL) {
                res_t add_res = ik_openai_conversation_add_msg(
                    repl->current->conversation,
                    sys_msg
                );
                if (is_err(&add_res)) {
                    ik_logger_warn(repl->shared->logger,
                        "Failed to add system message to conversation: %s",
                        add_res.msg);
                }
            }
        }
    }

    ik_logger_debug(repl->shared->logger,
        "Fresh install: wrote initial events for Agent 0");
}
```

### Part 3: Ensure config access

The function `ik_repl_restore_agents()` needs access to configuration. Verify that:
1. `repl->shared->cfg` is accessible (check `ik_shared_ctx_t` definition)
2. If not available, consider:
   - Adding `cfg` parameter to `ik_repl_restore_agents()`
   - Storing `cfg` reference in `ik_shared_ctx_t`

### Implementation Notes

1. **Error handling is non-fatal:** For fresh install events, log warnings but continue. The system can still function without initial clear/system events.

2. **Order matters:**
   - Clear event must be written first (establishes session marker)
   - System message written second (if configured)
   - Both scrollback and conversation must be updated for system message

3. **Message creation:** Use `ik_msg_create()` to create the system message for conversation, not direct struct allocation.

4. **Agent UUID:** Use `repl->current->uuid` as the agent_uuid parameter to `ik_db_message_insert()`.

5. **Session ID:** Use `repl->shared->session_id` which should be set by prior initialization.

## TDD Cycle

### Red
1. Run `make` to verify clean starting state
2. Run `make check` - all tests should pass
3. Write a test that verifies fresh install writes initial events

### Green
1. Add fresh install detection after Agent 0 restoration in agent_restore.c
2. Implement clear event writing
3. Implement system message writing (with config check)
4. Implement scrollback rendering for system message
5. Implement conversation addition for system message
6. Run `make` - should compile successfully
7. Run `make check` - all tests should pass
8. Run `make lint` - should pass

### Refactor
1. Consider extracting fresh install logic into a helper function if it becomes complex
2. Ensure error handling is consistent with the rest of agent_restore.c
3. Verify logging messages are appropriate level (debug for success, warn for failures)

## Unit Tests

Add tests to `tests/unit/repl/agent_restore_test.c`:

### Test cases to add:

1. **test_fresh_install_writes_clear_event**
   - Agent 0 with empty replay context
   - Verify clear event is inserted into database
   - Verify agent_uuid is set correctly

2. **test_fresh_install_writes_system_message**
   - Agent 0 with empty replay context
   - Config has `openai_system_message` set
   - Verify system event is inserted into database
   - Verify scrollback is updated
   - Verify conversation is updated

3. **test_fresh_install_no_system_message_when_not_configured**
   - Agent 0 with empty replay context
   - Config has NULL `openai_system_message`
   - Verify only clear event is written
   - Verify no system event is written

4. **test_existing_history_no_fresh_install_events**
   - Agent 0 with non-empty replay context (count > 0)
   - Verify no additional clear or system events are written
   - Verify existing history is processed normally

### Mock requirements:
- Mock `ik_db_message_insert()` to verify calls
- Mock `ik_event_render()` to verify scrollback updates
- Mock `ik_openai_conversation_add_msg()` to verify conversation updates

## Sub-agent Usage

Use sub-agents for:
- Searching for `cfg` access patterns in repl/shared contexts
- Running `make` and `make check` for quick feedback
- Verifying session_restore.c fresh install logic is fully replicated
- Checking that all error paths are handled

## Post-conditions
- `make` compiles successfully
- `make check` passes (all tests including new ones)
- `make lint` passes
- Fresh install case is handled in `ik_repl_restore_agents()`
- Clear event is written on fresh install
- System message is written and rendered if configured
- System message is added to conversation if configured
- Existing history case continues to work (no regression)
- Working tree is clean (all changes committed)

## Deviations

If you need to deviate from this plan (e.g., different location for fresh install handling, different approach to config access, additional error recovery), document the deviation and reasoning in your final report.
