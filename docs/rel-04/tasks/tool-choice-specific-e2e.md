# Task: E2E Test for tool_choice Specific Tool Mode

## Target
User story: 13-tool-choice-specific

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/mocking.md
- .agents/skills/coverage.md
- .agents/skills/testability.md

### Pre-read Docs
- docs/architecture.md
- docs/error_handling.md
- docs/anthropic_streaming_protocol.md
- rel-04/user-stories/13-tool-choice-specific.md

### Pre-read Source (patterns)
- src/openai/client.c (JSON serialization pattern with yyjson_mut_obj)
- src/openai/client_multi_request.c (request building and HTTP setup)
- src/openai/http_handler.c (HTTP handler patterns)

### Pre-read Tests (patterns)
- tests/unit/openai/client_http_mock_test.c (curl mocking pattern for fake responses)
- tests/integration/openai_mock_verification_test.c (stream callback and response accumulation)
- tests/unit/openai/client_multi_write_callback_test.c (SSE response handling)
- tests/integration/ (existing e2e test structure)
- tests/integration/test_tool_choice_required.c (similar test structure)

## Pre-conditions
- `make check` passes
- Task `tool-choice-required-e2e.md` completed
- Required mode e2e test passes
- Configuration mechanism for tool_choice exists

## Task
Create an end-to-end test that verifies `tool_choice: {"type": "function", "function": {"name": "glob"}}` works correctly:
1. User asks for file search
2. Request explicitly sets tool_choice to specific tool (glob)
3. Model must call the specified tool (glob)
4. Tool executes and returns results
5. Model summarizes results

This is more complex than the string modes because the JSON structure is an object.

**Note**: This is a verification test. The functionality is implemented in earlier tasks. If previous tasks are complete, the test should pass. If it fails, identify and fix gaps in the implementation.

## TDD Cycle

### Red
1. Create tests/integration/test_tool_choice_specific.c:
   - Configure request to use tool_choice specific "glob"
   - Mock OpenAI API to return glob tool call (model must use this tool)
   - Send user message "Find all C files in src/"
   - Verify outgoing request has `"tool_choice": {"type": "function", "function": {"name": "glob"}}`
   - Parse JSON to verify object structure (not string)
   - Verify glob tool is called
   - Mock final response with formatted results
   - Verify final assistant message is displayed
2. Run `make check` - expect pass

### Green
1. Verify test uses configuration mechanism to specify tool name
2. Verify JSON serialization creates object with correct nested structure
3. Verify type field is "function"
4. Verify function.name field matches specified tool
5. Run `make check` - expect pass

### Refactor
1. Consider parameterizing test to try different tools (file_read, grep)
2. Extract JSON verification utilities if complex
3. Verify test doesn't hardcode JSON strings (uses serialization)
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- E2E test verifies tool_choice specific tool mode
- Test covers full conversation flow from user story 13-tool-choice-specific
- JSON serialization correctly produces nested object structure
- 100% coverage of specific tool mode path
- All 4 tool_choice modes are now fully tested
