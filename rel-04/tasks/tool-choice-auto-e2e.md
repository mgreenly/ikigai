# Task: E2E Test for tool_choice Auto Mode

## Target
User story: 13-tool-choice-auto

## Agent
model: haiku

## Pre-conditions
- `make check` passes
- Task `request-with-tool-choice-param.md` completed
- Request builder accepts tool_choice parameter
- All existing stories (01-12) still pass

## Context
Read before starting:
- rel-04/user-stories/13-tool-choice-auto.md (expected behavior)
- tests/e2e/ directory (existing e2e test structure)
- rel-04/tasks/tool-loop-limit-e2e.md (example e2e test format)

## Task
Create an end-to-end test that verifies `tool_choice: "auto"` works correctly:
1. User asks for file search
2. Model decides to use glob tool
3. Tool executes and returns results
4. Model responds with formatted results

This is the default mode and should already work from Stories 01-02.

## TDD Cycle

### Red
1. Create tests/e2e/test_tool_choice_auto.c:
   - Mock OpenAI API to return tool call response
   - Send user message "Find all C files in src/"
   - Verify outgoing request has `"tool_choice": "auto"`
   - Verify glob tool is called
   - Mock final response with formatted results
   - Verify final assistant message is displayed
2. Run `make check` - expect pass (this should already work)

### Green
1. If test fails, verify request builder is using `ik_tool_choice_auto()` by default
2. If test passes, verify it's actually checking tool_choice field (make it fail by checking for "none")
3. Run `make check` - expect pass

### Refactor
1. Extract common e2e test utilities if duplicated
2. Verify test cleanup (no memory leaks, mocks restored)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- E2E test verifies tool_choice auto mode
- Test covers full conversation flow from user story 13-tool-choice-auto
- 100% coverage of auto mode path
