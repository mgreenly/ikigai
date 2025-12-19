# Task: Add Tool Executor Sub-Context to ik_agent_ctx_t

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 2 (Composition)

## Model

sonnet/extended (thread handling)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm
- patterns/factory

### Docs

- project/memory.md (talloc ownership patterns)
- rel-06/refactor.md (Section 1: Decompose ik_agent_ctx_t)

### Source Files

- src/agent_tool_executor.h (tool executor struct from Phase 1)
- src/agent.h (current ik_agent_ctx_t)
- src/agent.c (ik_agent_create, tool execution code)

### Test Patterns

- tests/unit/agent/agent_test.c (agent creation tests)
- tests/unit/agent/agent_tool_executor_test.c (tool executor tests from Phase 1)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_tool_executor_t` struct exists (from agent-tool-executor-struct.md)
- `ik_agent_identity_t *identity` field exists (from agent-compose-identity.md)
- `ik_agent_display_t *display` field exists (from agent-compose-display.md)
- `ik_agent_llm_t *llm` field exists (from agent-compose-llm.md)
- No `tool_executor` field in ik_agent_ctx_t yet

## Task

Add the `ik_agent_tool_executor_t *tool_executor` field to `ik_agent_ctx_t` and create it during agent initialization. The old tool execution fields remain for now - this is additive only.

### What

1. Add `#include "agent_tool_executor.h"` to agent.h
2. Add `ik_agent_tool_executor_t *tool_executor;` field to ik_agent_ctx_t
3. Create tool executor sub-context in ik_agent_create()
4. Create tool executor sub-context in ik_agent_restore()
5. Share mutex between legacy and new locations (reference same mutex)

### How

**In `src/agent.h`:**

```c
#include "agent_tool_executor.h"  // Add after existing includes

typedef struct ik_agent_ctx {
    // ... identity, display, llm fields ...

    // NEW: Focused sub-contexts
    ik_agent_identity_t *identity;
    ik_agent_display_t *display;
    ik_agent_llm_t *llm;
    ik_agent_tool_executor_t *tool_executor;

    // Tool execution state (per-agent) - LEGACY, use tool_executor->* after migration
    ik_tool_call_t *pending_tool_call;
    pthread_t tool_thread;
    pthread_mutex_t tool_thread_mutex;
    bool tool_thread_running;
    // ... rest of tool fields marked LEGACY ...

    // ... rest unchanged ...
} ik_agent_ctx_t;
```

**In `src/agent.c` (ik_agent_create):**

```c
res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                      const char *parent_uuid, ik_agent_ctx_t **out)
{
    // ... existing allocation, identity, display, llm code ...

    // Create tool executor sub-context
    agent->tool_executor = ik_agent_tool_executor_create(agent);

    // IMPORTANT: Remove legacy mutex initialization - use tool_executor's mutex
    // The legacy mutex field will reference the tool_executor's mutex
    // This is a temporary bridge during migration

    // For now, we still need the legacy mutex because existing code uses it
    // Initialize legacy mutex (will be removed when migration complete)
    int rc = pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    if (rc != 0) PANIC("Failed to initialize mutex");  // LCOV_EXCL_BR_LINE

    // Synchronize initial state
    agent->pending_tool_call = NULL;
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_ctx = NULL;
    agent->tool_thread_result = NULL;
    agent->tool_iteration_count = 0;

    // ... rest unchanged ...
}
```

**Update tool execution functions to synchronize:**

```c
void ik_agent_start_tool_execution(ik_agent_ctx_t *agent)
{
    // Lock both mutexes during transition period
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    pthread_mutex_lock_(&agent->tool_executor->mutex);

    agent->tool_thread_running = true;
    agent->tool_executor->running = true;

    agent->tool_executor->pending = agent->pending_tool_call;

    pthread_mutex_unlock_(&agent->tool_executor->mutex);
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // ... existing thread creation code ...
}

void ik_agent_complete_tool_execution(ik_agent_ctx_t *agent)
{
    // Lock both mutexes during transition period
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    pthread_mutex_lock_(&agent->tool_executor->mutex);

    // Synchronize state from tool_executor to legacy
    agent->tool_thread_running = agent->tool_executor->running;
    agent->tool_thread_complete = agent->tool_executor->complete;
    agent->tool_thread_result = agent->tool_executor->result;

    pthread_mutex_unlock_(&agent->tool_executor->mutex);
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // ... existing completion code ...
}
```

### Why

The tool executor is the most sensitive sub-context due to threading:

1. **Two mutexes during transition:** Both legacy and new mutex exist temporarily
2. **Lock ordering:** Always lock legacy first, then tool_executor to prevent deadlock
3. **State synchronization:** Both running/complete flags kept in sync
4. **Result sharing:** tool_thread_result points to tool_executor->result

After migration, only tool_executor->mutex will remain, and the legacy mutex will be removed.

**WARNING:** This is the highest-risk composition task. Careful testing with helgrind required.

## TDD Cycle

### Red

Update `tests/unit/agent/agent_test.c`:

```c
START_TEST(test_agent_create_has_tool_executor) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // New: verify tool executor sub-context exists
    ck_assert_ptr_nonnull(agent->tool_executor);
}
END_TEST

START_TEST(test_agent_tool_executor_state_matches_legacy) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // Verify initial state matches
    ck_assert(agent->tool_executor->running == agent->tool_thread_running);
    ck_assert(agent->tool_executor->complete == agent->tool_thread_complete);
    ck_assert_ptr_eq(agent->tool_executor->pending, agent->pending_tool_call);
}
END_TEST

START_TEST(test_agent_restore_has_tool_executor) {
    // ... setup with mock DB row ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(test_ctx, shared, &row, &agent);
    ck_assert(is_ok(res));

    ck_assert_ptr_nonnull(agent->tool_executor);
}
END_TEST

START_TEST(test_agent_has_running_tools_uses_both) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // Initially no running tools
    ck_assert(ik_agent_has_running_tools(agent) == false);

    // Set running via tool_executor
    pthread_mutex_lock_(&agent->tool_executor->mutex);
    agent->tool_executor->running = true;
    pthread_mutex_unlock_(&agent->tool_executor->mutex);

    // Should reflect in has_running_tools
    ck_assert(ik_agent_has_running_tools(agent) == true);
}
END_TEST
```

Run `make check` - expect failures.

### Green

1. Add include and field to `src/agent.h`
2. Update `ik_agent_create()` to create tool executor sub-context
3. Update `ik_agent_restore()` to create tool executor sub-context
4. Update tool execution functions to synchronize state
5. Run `make check` - all tests pass

### Verify

- `make check` passes
- `make lint` passes
- `make helgrind` passes (thread safety verified)
- Existing agent tests still pass
- Tool execution still works (integration test)

## Post-conditions

- `ik_agent_ctx_t` has `tool_executor` field
- `ik_agent_create()` creates tool executor sub-context
- `ik_agent_restore()` creates tool executor sub-context
- Tool execution functions synchronize state
- Both mutexes initialized (temporary during migration)
- All existing tests pass unchanged
- `make check` passes
- `make lint` passes
- `make helgrind` passes
- Working tree is clean (all changes committed)

## Rollback

If issues arise:
1. Remove `tool_executor` field from ik_agent_ctx_t
2. Remove tool executor creation from ik_agent_create/restore
3. Remove state synchronization from tool execution functions
4. Remove include
5. Remove new tests
6. `make check` should pass (back to original state)

## Sub-agent Usage

- Use haiku sub-agents for running `make check`, `make lint`, and `make helgrind` verification
- Use haiku sub-agents for git commits
