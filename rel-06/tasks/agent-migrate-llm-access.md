# Task: Migrate LLM Field Access

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 3 (Migration)

## Model

sonnet/extended (9 fields, many files - use sub-agents)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm

### Docs

- project/memory.md (talloc ownership)
- rel-06/refactor.md (Section 1: Decompose ik_agent_ctx_t)

### Source Files

- src/agent_llm.h (LLM struct with state enum)
- src/agent.h (current ik_agent_ctx_t with both legacy and llm fields)
- src/openai/client_multi.h (multi handle type)

### Test Patterns

- tests/unit/agent/agent_test.c (agent tests)
- tests/unit/openai/*.c (OpenAI tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_llm_t *llm` field exists in ik_agent_ctx_t
- Both legacy and llm fields populated during creation
- Identity field migration complete (from agent-migrate-identity-access.md)
- Display field migration complete (from agent-migrate-display-access.md)

## Task

Migrate all access to LLM fields from legacy to sub-context. This includes the state machine and response buffers.

### What

Replace all occurrences:

**State Machine:**
- `agent->state` -> `agent->llm->state`

**HTTP State:**
- `agent->multi` -> `agent->llm->multi`
- `agent->curl_still_running` -> `agent->llm->curl_still_running`

**Response Buffers:**
- `agent->assistant_response` -> `agent->llm->assistant_response`
- `agent->streaming_line_buffer` -> `agent->llm->streaming_line_buffer`
- `agent->http_error_message` -> `agent->llm->http_error_message`

**Response Metadata:**
- `agent->response_model` -> `agent->llm->response_model`
- `agent->response_finish_reason` -> `agent->llm->response_finish_reason`
- `agent->response_completion_tokens` -> `agent->llm->response_completion_tokens`

### How

**Step 1: Identify all files with LLM field access**

```bash
# Find all files accessing LLM fields
grep -rn "->state\|->multi\|->curl_still_running\|->assistant_response\|->streaming_line_buffer\|->http_error_message\|->response_model\|->response_finish_reason\|->response_completion_tokens" src/ tests/ \
    | grep -v "llm->" \
    | grep -v "spinner_state" \
    > /tmp/llm_access.txt
```

**Step 2: Handle state transition functions**

State transitions already synchronize both fields. After migration, remove synchronization and only update `llm->state`:

```c
// Before (transition period)
void ik_agent_transition_to_waiting_for_llm(ik_agent_ctx_t *agent)
{
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent->llm->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    // ...
}

// After (migration complete)
void ik_agent_transition_to_waiting_for_llm(ik_agent_ctx_t *agent)
{
    pthread_mutex_lock_(&agent->tool_executor->mutex);
    agent->llm->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&agent->tool_executor->mutex);
    // ...
}
```

**Step 3: Handle state enum location**

After migration, the state enum should be included from `agent_llm.h`:

```c
// Before - agent.h has its own enum definition
typedef enum {
    IK_AGENT_STATE_IDLE,
    IK_AGENT_STATE_WAITING_FOR_LLM,
    IK_AGENT_STATE_EXECUTING_TOOL
} ik_agent_state_t;

// After - include from agent_llm.h (remove duplicate from agent.h)
#include "agent_llm.h"  // provides ik_agent_state_t
```

**Step 4: Handle response buffer ownership**

Response buffers should be allocated as children of `llm`:

```c
// Before
agent->assistant_response = talloc_strdup(agent, response);

// After
agent->llm->assistant_response = talloc_strdup(agent->llm, response);
```

### Why

The LLM sub-context manages streaming state. Key migration considerations:

1. **State synchronization removal:** After migration, only `llm->state` needs updates
2. **Mutex usage:** State transitions still need mutex (now from tool_executor)
3. **Buffer ownership:** Response buffers move from agent to llm talloc tree
4. **Enum consolidation:** Remove duplicate enum from agent.h

### Files Likely Affected

Based on codebase analysis:
- `src/agent.c` - state transitions
- `src/repl.c` - state checks
- `src/repl_actions_llm.c` - LLM request/response handling
- `src/repl_event_handlers.c` - HTTP event handling
- `src/openai/client_multi.c` - multi handle operations
- `src/openai/client_multi_callbacks.c` - HTTP callbacks
- `tests/unit/agent/*.c` - agent tests
- `tests/unit/openai/*.c` - OpenAI tests

## TDD Cycle

### Red

No new tests needed - existing tests verify behavior. Run `make check` before migration to establish baseline.

### Green

**CRITICAL: Use sub-agents for parallel migration**

Spawn sub-agents to work in parallel:

1. **Sub-agent 1:** Migrate `src/agent.c` (state transitions)
2. **Sub-agent 2:** Migrate `src/repl.c`, `src/repl_actions_llm.c`
3. **Sub-agent 3:** Migrate `src/repl_event_handlers.c`
4. **Sub-agent 4:** Migrate `src/openai/client_multi*.c`
5. **Sub-agent 5:** Migrate test files

Each sub-agent:
1. Opens assigned file(s)
2. Replaces state field access
3. Replaces multi/curl_still_running access
4. Replaces response buffer access (update talloc parent)
5. Replaces response metadata access
6. Runs `make check` to verify

**Migration Order:**
1. First: state and multi (core LLM state)
2. Second: response buffers (update talloc parent)
3. Third: response metadata (read-only after response)

### Verify

After all sub-agents complete:
- `make check` passes
- `make lint` passes
- LLM requests work correctly (manual verification)
- State transitions correct

```bash
# Verify no remaining legacy access
grep -rn "agent->state[^_]" src/ tests/ | grep -v "llm->" | grep -v "spinner"
grep -rn "agent->multi" src/ tests/ | grep -v "llm->"
grep -rn "agent->assistant_response" src/ tests/ | grep -v "llm->"
```

## Post-conditions

- All LLM field access uses `agent->llm->*`
- State transitions only update `llm->state`
- Response buffers owned by llm (talloc parent)
- State enum removed from agent.h (only in agent_llm.h)
- All existing tests pass
- LLM requests work correctly
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If migration fails partway:
1. `git checkout -- .` to revert all changes
2. Debug failing test
3. Try migration again with fix

**Alternative:** Migrate by field group:
1. First commit: state field
2. Second commit: multi/curl_still_running
3. Third commit: response buffers
4. Fourth commit: response metadata

This enables granular rollback.

## Sub-agent Usage

**MANDATORY:** Use sub-agents for parallel file migration.

```
Sub-agent assignments:
- Agent A: src/agent.c (state transition functions)
- Agent B: src/repl.c, src/repl_actions_llm.c
- Agent C: src/repl_event_handlers.c
- Agent D: src/openai/client_multi.c, src/openai/client_multi_callbacks.c
- Agent E: tests/unit/agent/*.c, tests/unit/openai/*.c

Coordination:
1. Agent A completes state transitions first
2. Other agents work in parallel
3. Main agent removes duplicate enum from agent.h
4. Main agent runs final `make check`
```

**Note:** After all migrations, remove state synchronization code that was added in Phase 2.
