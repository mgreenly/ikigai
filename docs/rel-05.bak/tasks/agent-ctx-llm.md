# Task: Migrate LLM state to Agent Context

## Target
Phase 1: Agent Context Extraction - Step 5 (LLM fields migration)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/patterns/state-machine.md

## Pre-read Docs
- docs/backlog/shared-context-di.md (design document)
- docs/rel-05/scratch.md (ik_agent_ctx_t LLM fields)

## Pre-read Source (patterns)
- src/agent.h (current agent context)
- src/agent.c (current agent create)
- src/repl.h (LLM fields to migrate, ik_repl_state_t enum)
- src/repl_init.c (LLM state initialization)
- src/openai/openai_multi.h (ik_openai_multi)
- src/repl_event_handlers.c (LLM state usage)

## Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Display, input, and conversation fields already migrated
- `repl->agent` exists with conversation

## Task
Migrate LLM interaction state from `ik_repl_ctx_t` to `ik_agent_ctx_t`:
- `multi` - curl_multi handle for non-blocking HTTP
- `curl_still_running` - active transfer count
- `state` - REPL state machine (IDLE, WAITING_FOR_LLM, EXECUTING_TOOL)
- `assistant_response` - accumulated response during streaming
- `streaming_line_buffer` - incomplete line buffer
- `http_error_message` - error message if request failed
- `response_model` - model name from SSE
- `response_finish_reason` - finish reason from SSE
- `response_completion_tokens` - token count from SSE

Also move `ik_repl_state_t` enum to agent.h (rename to `ik_agent_state_t`).

After this task:
- Agent owns its LLM interaction state
- Each agent can independently stream from LLM
- Access pattern becomes `repl->agent->state`, etc.

## TDD Cycle

### Red
1. Update `src/agent.h`:
   - Add state enum (renamed from ik_repl_state_t):
     ```c
     typedef enum {
         IK_AGENT_STATE_IDLE,
         IK_AGENT_STATE_WAITING_FOR_LLM,
         IK_AGENT_STATE_EXECUTING_TOOL
     } ik_agent_state_t;
     ```
   - Add forward declaration:
     ```c
     struct ik_openai_multi;
     ```
   - Add fields:
     ```c
     // LLM interaction state (per-agent)
     struct ik_openai_multi *multi;
     int curl_still_running;
     ik_agent_state_t state;
     char *assistant_response;
     char *streaming_line_buffer;
     char *http_error_message;
     char *response_model;
     char *response_finish_reason;
     int32_t response_completion_tokens;
     ```

2. Update `tests/unit/agent/agent_test.c`:
   - Test `agent->state` is IK_AGENT_STATE_IDLE initially
   - Test `agent->multi` is NULL initially (created on first request)
   - Test `agent->curl_still_running` is 0 initially
   - Test response fields are NULL initially

3. Run `make check` - expect failures

### Green
1. Update `src/agent.c`:
   - Initialize LLM state:
     ```c
     agent->multi = NULL;  // Created on first LLM request
     agent->curl_still_running = 0;
     agent->state = IK_AGENT_STATE_IDLE;
     agent->assistant_response = NULL;
     agent->streaming_line_buffer = NULL;
     agent->http_error_message = NULL;
     agent->response_model = NULL;
     agent->response_finish_reason = NULL;
     agent->response_completion_tokens = 0;
     ```

2. Update `src/repl.h`:
   - Remove `ik_repl_state_t` enum (moved to agent.h as ik_agent_state_t)
   - Remove all LLM fields listed above

3. Update `src/repl_init.c`:
   - Remove LLM state initialization (now in agent_create)

4. Update ALL files that access LLM state:
   - Change `repl->multi` to `repl->agent->multi`
   - Change `repl->curl_still_running` to `repl->agent->curl_still_running`
   - Change `repl->state` to `repl->agent->state`
   - Change `IK_REPL_STATE_*` to `IK_AGENT_STATE_*`
   - Change all response field accesses similarly

5. Update state transition functions in repl.h/repl.c:
   - `ik_repl_transition_to_*` functions now modify `repl->agent->state`
   - Consider renaming to `ik_agent_transition_to_*` (or keep repl_ prefix for now)

6. Run `make check` - expect pass

### Refactor
1. Verify curl_multi handle ownership (created when needed, freed with agent)
2. Verify state transitions work correctly with new structure
3. Verify streaming response accumulation works
4. Verify no direct LLM field access remains in repl_ctx
5. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- LLM fields are in `ik_agent_ctx_t`, not `ik_repl_ctx_t`
- `ik_agent_state_t` enum defined in agent.h
- State machine works with agent context
- All LLM state access uses `repl->agent->*` pattern
- 100% test coverage maintained
- Working tree is clean (all changes committed)
