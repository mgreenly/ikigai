# Task: Create ik_agent_llm_t Struct and Factory

## Target

Refactoring #1: Decompose `ik_agent_ctx_t` God Object - LLM Interaction Sub-context

## Pre-read Skills

- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/ddd.md
- .agents/skills/di.md
- .agents/skills/patterns/context-struct.md
- .agents/skills/patterns/factory.md

## Pre-read Source (patterns)

- src/agent.h (lines 89-103 - LLM-related fields in ik_agent_ctx_t)
- src/agent.c (lines 67-82 - LLM state initialization)
- src/openai/client.h (conversation interface)
- src/openai/client_multi.h (curl multi interface)

## Pre-read Tests (patterns)

- tests/unit/agent/agent_test.c (LLM field initialization tests)
- tests/unit/agent/agent_identity_test.c (pattern from previous task)
- tests/unit/agent/agent_display_test.c (pattern from previous task)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Tasks `agent-identity-struct` and `agent-display-struct` are complete
- `ik_agent_identity_t` and `ik_agent_display_t` structs exist

## Task

Extract LLM interaction and conversation fields from `ik_agent_ctx_t` into a new `ik_agent_llm_t` struct with its own factory function.

### What

Create a new struct `ik_agent_llm_t` containing:

**Conversation State (3 fields):**
- `ik_openai_conversation_t *conversation` - Message array for LLM
- `ik_mark_t **marks` - Conversation checkpoint marks
- `size_t mark_count` - Number of marks

**HTTP/Curl State (2 fields):**
- `struct ik_openai_multi *multi` - Curl multi handle
- `int curl_still_running` - Active transfer count

**Agent State Machine (1 field):**
- `ik_agent_state_t state` - Current agent state (IDLE, WAITING_FOR_LLM, EXECUTING_TOOL)

**Response Buffers (6 fields):**
- `char *assistant_response` - Accumulated assistant response
- `char *streaming_line_buffer` - Buffer for incomplete SSE lines
- `char *http_error_message` - Error message from failed requests
- `char *response_model` - Model that generated response
- `char *response_finish_reason` - Why response ended (stop, tool_calls, etc.)
- `int32_t response_completion_tokens` - Token count for response

**Total: 12 fields** (adjusted from initial count after detailed analysis)

### How

1. In `src/agent.h`:
   - Add `ik_agent_llm_t` struct definition AFTER `ik_agent_display_t`, BEFORE `ik_agent_ctx_t`
   - Add factory declaration: `res_t ik_agent_llm_create(TALLOC_CTX *ctx, ik_agent_llm_t **out);`

2. In `src/agent.c` (or new `src/agent_llm.c`):
   - Implement `ik_agent_llm_create()`:
     - Allocate `ik_agent_llm_t` under ctx
     - Create conversation via `ik_openai_conversation_create()`
     - Initialize marks to NULL, mark_count to 0
     - Create multi via `ik_openai_multi_create()`
     - PANIC if multi creation fails (matches current behavior)
     - Set curl_still_running to 0
     - Set state to IK_AGENT_STATE_IDLE
     - Set all response buffers to NULL
     - Set response_completion_tokens to 0

### Why

The LLM fields (12 total) represent a distinct concern: interaction with language models. They are:
- All related to LLM API communication
- Managed together during streaming responses
- Include the agent state machine (which governs LLM interaction)
- Independent of identity, display, and tool execution details

Extracting them enables:
- Testable LLM state management without full agent
- Clear separation of API concerns
- Easier future support for multiple LLM providers

## TDD Cycle

### Red

Create `tests/unit/agent/agent_llm_test.c`:

1. Test `ik_agent_llm_create()` succeeds
2. Test llm->conversation is non-NULL
3. Test llm->marks is NULL initially
4. Test llm->mark_count is 0 initially
5. Test llm->multi is non-NULL
6. Test llm->curl_still_running is 0 initially
7. Test llm->state is IK_AGENT_STATE_IDLE initially
8. Test llm->assistant_response is NULL initially
9. Test llm->streaming_line_buffer is NULL initially
10. Test llm->http_error_message is NULL initially
11. Test llm->response_model is NULL initially
12. Test llm->response_finish_reason is NULL initially
13. Test llm->response_completion_tokens is 0 initially
14. Test llm is allocated under provided parent context

Run `make check` - expect compilation failure (struct doesn't exist yet)

### Green

1. Add struct definition to `src/agent.h`
2. Add factory declaration to `src/agent.h`
3. Implement factory in `src/agent.c`
4. Add test file to `tests/unit/agent/meson.build`
5. Run `make check` - expect pass

### Refactor

1. Verify PANIC on multi creation failure matches existing behavior
2. Verify conversation creation uses correct talloc parent
3. Run `make lint` - verify clean

## Post-conditions

- `make check` passes
- `make lint` passes
- `ik_agent_llm_t` struct exists in `src/agent.h`
- `ik_agent_llm_create()` implemented
- Unit tests cover all LLM fields
- Working tree is clean (all changes committed)

## Sub-agent Usage

- Use sub-agents to search for LLM field usages: `grep -r "agent->conversation\|agent->multi\|agent->state\|agent->assistant_response" src/`
- Identify callers that will need migration

## Notes

The agent state machine (`ik_agent_state_t`) is included here because it primarily governs LLM interaction flow (IDLE -> WAITING_FOR_LLM -> EXECUTING_TOOL cycle). The state transitions are about whether we're waiting for LLM response.

The `input_text` and `input_text_len` fields that reference input buffer are NOT included here - they belong with input state (kept in main agent ctx).
