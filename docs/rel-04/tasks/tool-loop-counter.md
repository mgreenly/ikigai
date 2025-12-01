# Task: Tool Loop Iteration Counter

## Target
User story: 11-tool-loop-limit-reached

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/mocking.md
- .agents/skills/coverage.md
- .agents/skills/quality.md
- .agents/skills/testability.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/architecture.md
- docs/v1-conversation-management.md
- docs/naming.md
- rel-04/tasks/multi-tool-loop.md (related task: existing loop structure)
- rel-04/user-stories/11-tool-loop-limit-reached.md (user story: walkthrough steps 1-10)

### Pre-read Source (patterns)
- src/repl.c or src/repl_actions.c (conversation loop implementation)
- src/repl.h (REPL context structure for counter variable)
- src/repl_callbacks.c (finish_reason metadata handling)

### Pre-read Tests (patterns)
- tests/unit/repl/handle_request_success_test.c (test patterns for conversation loop)
- tests/unit/repl/repl_http_completion_callback_test.c (API response handling patterns)

## Pre-conditions
- `make check` passes
- Task `tool-config-fields.md` completed (Story 02)
- `ik_cfg_t` has `max_tool_turns` field accessible via `repl->cfg`
- Multi-tool conversation loop works (Story 04)

## Task
Add iteration counting to the conversation loop. Track how many times tool calls have been executed in the current loop and detect when the `max_tool_turns` config limit is reached.

The counter should:
1. Initialize to 0 at the start of handling a user request
2. Increment each time finish_reason is "tool_calls" and tools are executed
3. Be checked before sending the next API request
4. Trigger limit-reached behavior when counter reaches `repl->cfg->max_tool_turns`

IMPORTANT: The counter must be scoped to the entire user request/conversation turn, NOT to an inner loop function. It needs to persist across multiple API round-trips within a single user request to accurately count the total number of tool call iterations.

## TDD Cycle

### Red
1. Add integration test to existing conversation loop test suite:
   - Use test config with `max_tool_turns` set to 3 for easy verification
   - Mock API sequence with 4 tool_calls responses (exceeds limit of 3)
   - Verify first 3 tool calls execute normally
   - Verify 4th tool call is NOT sent
   - Verify limit detection happens after 3rd iteration
2. Run `make check` - expect test failure (counter doesn't exist yet)

### Green
1. Locate conversation loop in repl.c or repl_actions.c
2. Add counter variable scoped to the user request handler (e.g., `int tool_iteration = 0`)
   - NOTE: Counter must be scoped to the ENTIRE user request/conversation turn
   - It should persist across ALL iterations of the tool loop
   - It counts the total number of tool call rounds in the current request
   - DO NOT scope it to the inner loop function - it must survive across multiple API round-trips
3. Initialize counter to 0 at start of request handling
4. Increment counter after each tool execution (each time finish_reason is "tool_calls")
5. Add check: `if (tool_iteration >= repl->cfg->max_tool_turns)` before sending next API request
6. Run `make check` - expect pass

### Refactor
1. Consider extracting limit check to separate function for clarity
2. Ensure counter is properly scoped (local to request handling)
3. Add comments explaining counter purpose
4. Verify counter resets properly for new user requests
5. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Conversation loop tracks tool call iterations
- Loop detects when `repl->cfg->max_tool_turns` limit is reached
- Counter resets for each new user request
- 100% test coverage for new code
