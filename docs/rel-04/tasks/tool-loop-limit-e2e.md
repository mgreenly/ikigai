# Task: Tool Loop Limit End-to-End Test

## Target
User story: 11-tool-loop-limit-reached

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/mocking.md
- .agents/skills/database.md

### Pre-read Docs
- docs/v1-conversation-management.md
- docs/v1-llm-integration.md
- rel-04/user-stories/11-tool-loop-limit-reached.md

### Pre-read Source (patterns)
- src/repl.c (conversation loop handling)
- src/openai/client.c (API request handling)

### Pre-read Tests (patterns)
- tests/integration/message_integration_test.c (integration test patterns)
- tests/unit/repl/repl_run_basic_test.c (repl mocking patterns)
- rel-04/tasks/file-read-error-e2e.md (example e2e test structure)
- rel-04/tasks/bash-command-error-e2e.md (example e2e test structure)

## Pre-conditions
- `make check` passes
- Task `tool-choice-none-on-limit.md` completed
- All components for limit handling implemented:
  - MAX_TOOL_TURNS config exists
  - Loop counter tracks iterations
  - Limit metadata added to tool results
  - tool_choice set to "none" when limit reached

## Task
Create an end-to-end integration test that verifies the complete tool loop limit behavior:
1. User makes request that triggers multiple tool calls
2. Model responds with tool_calls (iteration 1)
3. Client executes, model responds with tool_calls (iteration 2)
4. Client executes, model responds with tool_calls (iteration 3)
5. Client executes, detects limit reached
6. Client adds limit metadata to tool result
7. Client sends final request with tool_choice: "none"
8. Model provides summary response with finish_reason: "stop"
9. All messages persisted to database
10. Scrollback displays final response

**Note**: This is a verification test. The functionality is implemented in earlier tasks. If previous tasks are complete, the test should pass. If it fails, identify and fix gaps in the implementation.

## TDD Cycle

### Red
1. Create `tests/integration/tool_loop_limit_test.c`:
   - Mock OpenAI client to return sequence:
     - Response A: grep tool call (finish_reason: "tool_calls")
     - Response B: grep tool call (finish_reason: "tool_calls")
     - Response C: grep tool call (finish_reason: "tool_calls")
     - Response D: text content acknowledging limit (finish_reason: "stop")
   - Mock grep tool to return simple output each time
   - Send user request: "Keep searching for errors in every file"
   - Verify:
     - 3 tool calls executed (not 4)
     - 3rd tool result contains limit metadata
     - 4th request has tool_choice: "none"
     - Final response displayed
     - Conversation has 8 messages (user, asst, tool, asst, tool, asst, tool, asst)
     - All messages in database
2. Run `make check` - expect failure if any component missing

### Green
1. Wire together all limit handling components in conversation loop:
   - Initialize counter
   - Increment after each tool execution
   - Detect when counter == MAX_TOOL_TURNS
   - Add limit metadata to tool result
   - Set tool_choice to "none" in next request
   - Ensure loop terminates after final response
2. Run `make check` - expect pass

### Refactor
1. Review conversation loop for clarity and maintainability
2. Ensure proper cleanup of limit-related state between requests
3. Check error handling: what if model ignores tool_choice: "none"?
4. Verify logging/debugging output is helpful
5. Consider edge cases:
   - Limit reached on last file in search
   - Model provides stop before reaching limit
6. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- End-to-end test verifies complete limit behavior
- Conversation loop properly integrates all limit components
- Story 11 user story works as specified
- 100% test coverage for integration test
