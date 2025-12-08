# Task: E2E Test for tool_choice Required Mode

## Target
User story: 13-tool-choice-required

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/mocking.md
- .agents/skills/testability.md
- .agents/skills/quality.md
- .agents/skills/coverage.md

### Pre-read Docs
- docs/architecture.md
- docs/error_testing.md
- rel-04/user-stories/13-tool-choice-required.md

### Pre-read Source (patterns)
- src/openai/client.h (request structure and functions)
- src/openai/client_multi_request.c (request building pattern with mocking)
- src/wrapper.h (MOCKABLE pattern for curl overrides)

### Pre-read Tests (patterns)
- tests/unit/openai/client_http_mock_test.c (curl mocking pattern)
- tests/integration/openai_mock_verification_test.c (OpenAI API mock setup pattern)
- tests/integration/message_integration_test.c (integration test structure with setup/teardown)
- tests/integration/test_tool_choice_none.c (similar test structure)

## Pre-conditions
- `make check` passes
- Task `tool-choice-none-e2e.md` completed
- None mode e2e test passes
- Configuration mechanism for tool_choice exists

## Task
Create an end-to-end test that verifies `tool_choice: "required"` works correctly:
1. User asks for file search
2. Request explicitly sets `tool_choice: "required"`
3. Model must call a tool (cannot respond with text only)
4. Tool executes and returns results
5. Model summarizes results

**Note**: This is a verification test. The functionality is implemented in earlier tasks. If previous tasks are complete, the test should pass. If it fails, identify and fix gaps in the implementation.

## TDD Cycle

### Red
1. Create tests/integration/test_tool_choice_required.c:
   - Configure request to use tool_choice required
   - Mock OpenAI API to return tool call response (model must use tool)
   - Send user message "Find all C files in src/"
   - Verify outgoing request has `"tool_choice": "required"`
   - Verify tool execution occurs
   - Mock final response with formatted results
   - Verify final assistant message is displayed
2. Run `make check` - expect pass

### Green
1. Verify test uses configuration mechanism from previous task
2. Verify request serialization includes "required" string
3. Run `make check` - expect pass

### Refactor
1. Extract common setup code with other tool_choice e2e tests
2. Verify test covers edge cases (what if model violates required?)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- E2E test verifies tool_choice required mode
- Test covers full conversation flow from user story 13-tool-choice-required
- 100% coverage of required mode path
