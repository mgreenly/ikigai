# Task: Set tool_choice to "none" When Limit Reached

## Target
User story: 11-tool-loop-limit-reached

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- Task `tool-result-limit-metadata.md` completed
- Tool result includes limit metadata when limit is reached
- Request builder works (Story 01: request-with-tools.md completed)

## Context
Read before starting:
- src/openai/client.c or src/openai/request.c (request construction)
- src/repl.c or src/repl_actions.c (conversation loop)
- rel-04/user-stories/11-tool-loop-limit-reached.md (see Request D)
- rel-04/tasks/request-with-tools.md (existing request builder)

## Task
When the tool iteration limit is reached, the final API request must set `tool_choice: "none"` to force the model to respond with text instead of making more tool calls.

Normal requests use `tool_choice: "auto"`. After reaching the limit and adding the limit metadata to the tool result, the next (and final) request should explicitly set `tool_choice: "none"`.

## TDD Cycle

### Red
1. Add test to request builder test suite:
   - Build request after limit reached (with limit metadata in last tool result)
   - Verify request JSON contains `"tool_choice": "none"`
   - Build normal request (before limit)
   - Verify request JSON contains `"tool_choice": "auto"`
2. Run `make check` - expect test failure (always sends "auto")

### Green
1. Locate request builder function in src/openai/client.c or request.c
2. Add parameter or state to indicate limit has been reached
   - Option A: Pass boolean flag `limit_reached` to request builder
   - Option B: Check if last tool result has limit metadata
3. Conditionally set tool_choice:
   - If limit reached: `"tool_choice": "none"`
   - Otherwise: `"tool_choice": "auto"`
4. Update conversation loop to pass limit state to request builder
5. Run `make check` - expect pass

### Refactor
1. Consider extracting tool_choice logic to helper function
2. Ensure parameter name is clear (e.g., `force_stop_tools`, `limit_reached`)
3. Verify JSON serialization is correct for both cases
4. Check if any other request fields need adjustment when limit is reached
5. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Request builder accepts limit_reached indicator
- tool_choice is "none" when limit reached
- tool_choice is "auto" for normal requests
- 100% test coverage for new code
