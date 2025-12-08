# Task: Send Follow-up Request and Complete Tool Loop

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/mocking.md
- .agents/skills/testability.md
- .agents/skills/naming.md
- .agents/skills/quality.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/error_handling.md
- rel-04/user-stories/02-single-glob-call.md (user story - full walkthrough)
- rel-04/README.md (loop limits and execution model)

### Pre-read Source (patterns)
- src/openai/client.c (API request submission)
- src/repl_callbacks.c (streaming callback and completion handling)
- src/repl.h (REPL context structure)
- src/openai/client_multi.h (completion callback typedef)

### Pre-read Tests (patterns)
- tests/unit/repl/repl_http_completion_callback_test.c (completion callback testing pattern)
- tests/integration/repl_test.c (end-to-end REPL integration testing)

## Pre-conditions
- `make check` passes
- finish_reason detection works (task `tool-loop-finish-detection.md` completed)
- Conversation state mutation works (task `tool-loop-state-mutation.md` completed)
- Tool execution completes and adds messages to conversation

## Task
Integrate the complete tool loop by sending follow-up requests:
1. In completion callback, check finish_reason via accessor
2. If finish_reason == "tool_calls":
   - Send follow-up API request with updated conversation
   - Return to streaming callbacks
3. If finish_reason == "stop":
   - Complete the turn normally
4. Continue loop until finish_reason: "stop"

This task completes the end-to-end tool loop integration for Story 02. Loop limits and iteration tracking are added in Story 11.

## TDD Cycle

### Red
1. Create integration test for complete tool loop:
   - Mock API to return Response A (finish_reason: "tool_calls")
   - Verify tool execution is triggered
   - Verify conversation state is updated
   - Verify Request B is sent with correct messages
   - Mock API to return Response B (finish_reason: "stop")
   - Verify final response is displayed
2. Run `make check` - expect test failure (loop not wired)

### Green
1. Identify the completion callback where finish_reason is available
2. Add check for finish_reason using accessor from task 1
3. When finish_reason == "tool_calls":
   - Call conversation state mutation (from task 2)
   - Send follow-up API request
4. When finish_reason == "stop", complete normally
5. Wire up the loop so it continues until "stop"
6. Run `make check` - expect pass

### Refactor
1. Consider extracting tool loop logic into separate function
2. Ensure error handling for API failures during loop
3. Verify that streaming callbacks work correctly on follow-up requests
4. Check memory management for multiple API requests
5. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Single glob tool call executes end-to-end
- Follow-up request is sent automatically after tool execution
- Loop continues until finish_reason: "stop"
- Conversation state maintains full history
- 100% test coverage for new code
