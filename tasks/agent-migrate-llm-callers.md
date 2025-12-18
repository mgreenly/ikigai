# Task: Migrate LLM Field Callers

## Target

Refactoring #1: Decompose `ik_agent_ctx_t` God Object - LLM Field Migration

## Pre-read Skills

- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Source (patterns)

- src/agent.h (ik_agent_ctx_t with embedded ik_agent_llm_t)
- src/agent.c (LLM initialization and state transitions)
- src/repl_actions_llm.c (LLM request handling)
- src/openai/client.c (conversation operations)
- src/completion.c
- src/repl_viewport.c
- src/marks.c
- src/commands.c

## Pre-read Tests (patterns)

- tests/unit/agent/agent_test.c (updated accessor patterns)
- tests/unit/agent/agent_llm_test.c

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Task `agent-migrate-display-callers` is complete
- All display field migrations done

## Task

Migrate all production code (src/*.c) to use the new LLM field accessor pattern.

### What

Update all source files that access LLM fields to use the new path:

| Old Pattern | New Pattern |
|-------------|-------------|
| `agent->conversation` | `agent->llm.conversation` |
| `agent->marks` | `agent->llm.marks` |
| `agent->mark_count` | `agent->llm.mark_count` |
| `agent->multi` | `agent->llm.multi` |
| `agent->curl_still_running` | `agent->llm.curl_still_running` |
| `agent->state` | `agent->llm.state` |
| `agent->assistant_response` | `agent->llm.assistant_response` |
| `agent->streaming_line_buffer` | `agent->llm.streaming_line_buffer` |
| `agent->http_error_message` | `agent->llm.http_error_message` |
| `agent->response_model` | `agent->llm.response_model` |
| `agent->response_finish_reason` | `agent->llm.response_finish_reason` |
| `agent->response_completion_tokens` | `agent->llm.response_completion_tokens` |

### How

1. **Discovery Phase** (use sub-agents):

   Search BOTH access patterns - direct agent and via repl->current:
   ```bash
   # Direct agent access
   grep -rn "agent->conversation\b" src/
   grep -rn "agent->marks\|agent->mark_count" src/
   grep -rn "agent->multi\b" src/
   grep -rn "agent->curl_still_running" src/
   grep -rn "agent->state\b" src/
   grep -rn "agent->assistant_response\|agent->streaming_line_buffer" src/
   grep -rn "agent->http_error_message\|agent->response_" src/

   # Indirect via repl->current (accounts for 80%+ of callsites)
   grep -rn "repl->current->conversation\b" src/
   grep -rn "repl->current->marks\|repl->current->mark_count" src/
   grep -rn "repl->current->multi\b" src/
   grep -rn "repl->current->curl_still_running" src/
   grep -rn "repl->current->state\b" src/
   grep -rn "repl->current->assistant_response\|repl->current->streaming_line_buffer" src/
   grep -rn "repl->current->http_error_message\|repl->current->response_" src/
   ```

2. **Update each file**:
   - For each file with matches, update the accessor pattern
   - Pay special attention to `agent->state` as it's a common field name
   - Run `make check` after each file

3. **Expected files with changes**:
   - `src/agent.c` - State transitions
   - `src/repl_actions_llm.c` - LLM request handling
   - `src/repl_callbacks.c` - HTTP response handling
   - `src/repl_event_handlers.c` - Stream processing
   - `src/commands_mark.c` - Mark operations
   - `src/db/message.c` - Message persistence

### Why

LLM fields are critical for API interaction. This migration:
- Separates LLM concerns from other agent state
- Groups related fields (state machine, response buffers)
- Enables future multi-LLM support

## TDD Cycle

### Red

Code should fail to compile if old field paths are removed.

### Green

1. Run grep to find all callers (expect 40-60 locations)
2. Group by functionality (state transitions, conversation, HTTP)
3. Update one group at a time
4. Run `make check` after each group

### Refactor

1. Verify no old patterns remain
2. Run `make lint` - verify clean
3. Verify state transitions still work correctly

## Post-conditions

- `make check` passes
- `make lint` passes
- No source files use `agent->conversation` (use `agent->llm.conversation`)
- No source files use `agent->marks` (use `agent->llm.marks`)
- No source files use `agent->multi` (use `agent->llm.multi`)
- No source files use `agent->state` for agent state (use `agent->llm.state`)
- No source files use `agent->assistant_response` (use `agent->llm.assistant_response`)
- No source files use other LLM response fields directly
- Working tree is clean (all changes committed)

## Sub-agent Usage

**RECOMMENDED: Use sub-agents for this task**

LLM fields have moderate usage (estimated 40-60 locations). Sub-agents should:
1. Handle state transition code carefully
2. Process HTTP handling code as a unit
3. Handle mark operations separately

Pattern for sub-agent:
```
For file "repl_actions_llm.c":
  - Read file
  - Replace agent->conversation with agent->llm.conversation
  - Replace agent->state with agent->llm.state
  - Replace agent->assistant_response with agent->llm.assistant_response
  - etc.
  - Write file
  - Run make check
```

## Sub-Agent Execution Strategy

This task has 118 callsites across multiple files. Use sub-agents to parallelize:

1. Spawn one sub-agent per file group (e.g., state transition files, HTTP handling files, mark files)
2. Each sub-agent:
   - Grep for all patterns in assigned files
   - Update field access to new pattern (e.g., `agent->conversation` → `agent->llm.conversation`)
   - Verify with `make check`
   - Report completion

### Suggested Batches:
- **Batch 1:** src/agent.c (state transitions)
- **Batch 2:** src/repl_actions_llm.c, src/repl_callbacks.c (LLM request handling)
- **Batch 3:** src/commands_mark.c, src/marks.c (mark operations)
- **Batch 4:** Remaining files (completion.c, repl_viewport.c, commands.c)

## Notes

**Critical: `agent->state` Disambiguation**

The field `agent->state` is the agent state machine (IDLE, WAITING_FOR_LLM, EXECUTING_TOOL). Be careful not to confuse it with:
- Other `state` fields in different contexts
- State enum values like `IK_AGENT_STATE_IDLE`

The grep pattern `agent->state\b` should find only agent state accesses.

**State Transitions**

The state transition functions in `src/agent.c` already operate on agent context. After migration:
```c
void ik_agent_transition_to_waiting_for_llm(ik_agent_ctx_t *agent)
{
    pthread_mutex_lock_(&agent->tool.tool_thread_mutex);  // Note: tool mutex!
    agent->llm.state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&agent->tool.tool_thread_mutex);
    // ...
}
```

Note the mutex is in `tool` sub-context but state is in `llm` sub-context. This cross-reference is intentional - the mutex protects state transitions from tool thread interference.

**Conversation Null Checks**

Some code may check `agent->conversation != NULL`. These become `agent->llm.conversation != NULL`.
