# Task: Add LLM Sub-Context to ik_agent_ctx_t

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 2 (Composition)

## Model

sonnet/extended (LLM state)

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

- src/agent_llm.h (LLM struct from Phase 1)
- src/agent.h (current ik_agent_ctx_t)
- src/agent.c (ik_agent_create, state transitions)
- src/openai/client_multi.h (multi handle)

### Test Patterns

- tests/unit/agent/agent_test.c (agent creation tests)
- tests/unit/agent/agent_llm_test.c (LLM tests from Phase 1)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_llm_t` struct exists (from agent-llm-struct.md)
- `ik_agent_identity_t *identity` field exists (from agent-compose-identity.md)
- `ik_agent_display_t *display` field exists (from agent-compose-display.md)
- No `llm` field in ik_agent_ctx_t yet

## Task

Add the `ik_agent_llm_t *llm` field to `ik_agent_ctx_t` and create it during agent initialization. The old LLM fields remain for now - this is additive only.

### What

1. Add `#include "agent_llm.h"` to agent.h
2. Add `ik_agent_llm_t *llm;` field to ik_agent_ctx_t
3. Create LLM sub-context in ik_agent_create()
4. Create LLM sub-context in ik_agent_restore()
5. Synchronize LLM fields between legacy and new locations

### How

**In `src/agent.h`:**

```c
#include "agent_llm.h"  // Add after existing includes

typedef struct ik_agent_ctx {
    // ... identity and display fields ...

    // NEW: Focused sub-contexts
    ik_agent_identity_t *identity;
    ik_agent_display_t *display;
    ik_agent_llm_t *llm;

    // LLM interaction state (per-agent) - LEGACY, use llm->* after migration
    struct ik_openai_multi *multi;
    int curl_still_running;
    ik_agent_state_t state;
    char *assistant_response;
    // ... rest of LLM fields marked LEGACY ...

    // ... rest unchanged ...
} ik_agent_ctx_t;
```

**In `src/agent.c` (ik_agent_create):**

```c
res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                      const char *parent_uuid, ik_agent_ctx_t **out)
{
    // ... existing allocation, identity, and display code ...

    // Create LLM sub-context
    agent->llm = ik_agent_llm_create(agent);

    // Synchronize state with legacy field
    agent->state = agent->llm->state;  // Both start at IDLE

    // Other LLM fields start NULL/0 - synchronized on first use
    agent->multi = NULL;  // Legacy (will use llm->multi)
    agent->curl_still_running = 0;
    agent->assistant_response = NULL;
    agent->streaming_line_buffer = NULL;
    agent->http_error_message = NULL;
    agent->response_model = NULL;
    agent->response_finish_reason = NULL;
    agent->response_completion_tokens = 0;

    // ... rest unchanged ...
}
```

**Update state transition functions to synchronize:**

```c
void ik_agent_transition_to_waiting_for_llm(ik_agent_ctx_t *agent)
{
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent->llm->state = IK_AGENT_STATE_WAITING_FOR_LLM;  // Sync
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // ... rest unchanged ...
}

void ik_agent_transition_to_idle(ik_agent_ctx_t *agent)
{
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->state = IK_AGENT_STATE_IDLE;
    agent->llm->state = IK_AGENT_STATE_IDLE;  // Sync
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // ... rest unchanged ...
}

// Similarly for other transition functions
```

### Why

The LLM sub-context manages HTTP streaming state. Key considerations:

1. **State synchronization:** Both `agent->state` and `agent->llm->state` must stay in sync during transition
2. **Lazy multi handle:** The curl multi handle is created on first request, not at agent creation
3. **Response buffer ownership:** Buffers allocated during streaming - owner TBD during migration
4. **State enum duplication:** Both agent.h and agent_llm.h define ik_agent_state_t during migration

After migration, the state enum in agent.h will be removed and only agent_llm.h's definition will remain.

## TDD Cycle

### Red

Update `tests/unit/agent/agent_test.c`:

```c
START_TEST(test_agent_create_has_llm) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // New: verify LLM sub-context exists
    ck_assert_ptr_nonnull(agent->llm);
}
END_TEST

START_TEST(test_agent_llm_state_matches_legacy) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // Verify state is synchronized
    ck_assert_int_eq(agent->llm->state, agent->state);
    ck_assert_int_eq(agent->llm->state, IK_AGENT_STATE_IDLE);
}
END_TEST

START_TEST(test_agent_transition_syncs_llm_state) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    ik_agent_transition_to_waiting_for_llm(agent);

    // Both should be updated
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert_int_eq(agent->llm->state, IK_AGENT_STATE_WAITING_FOR_LLM);
}
END_TEST

START_TEST(test_agent_restore_has_llm) {
    // ... setup with mock DB row ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(test_ctx, shared, &row, &agent);
    ck_assert(is_ok(res));

    ck_assert_ptr_nonnull(agent->llm);
    ck_assert_int_eq(agent->llm->state, IK_AGENT_STATE_IDLE);
}
END_TEST
```

Run `make check` - expect failures.

### Green

1. Add include and field to `src/agent.h`
2. Update `ik_agent_create()` to create LLM sub-context
3. Update `ik_agent_restore()` to create LLM sub-context
4. Update state transition functions to synchronize state
5. Run `make check` - all tests pass

### Verify

- `make check` passes
- `make lint` passes
- Existing agent tests still pass
- State transitions update both legacy and llm->state
- LLM requests still work (manual verification)

## Post-conditions

- `ik_agent_ctx_t` has `llm` field
- `ik_agent_create()` creates LLM sub-context
- `ik_agent_restore()` creates LLM sub-context
- State transition functions synchronize state
- All existing tests pass unchanged
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If issues arise:
1. Remove `llm` field from ik_agent_ctx_t
2. Remove LLM creation from ik_agent_create/restore
3. Remove state synchronization from transition functions
4. Remove include
5. Remove new tests
6. `make check` should pass (back to original state)

## Sub-agent Usage

- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits
