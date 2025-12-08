# Task: End-to-End Test for File Read Error Handling

## Target
User story: 08-file-not-found-error

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/mocking.md
- .agents/skills/database.md
- .agents/skills/testability.md
- .agents/skills/quality.md

### Pre-read Docs
- docs/error_handling.md
- docs/error_patterns.md
- docs/error_testing.md
- docs/return_values.md
- rel-04/user-stories/08-file-not-found-error.md (user story - full walkthrough)

### Pre-read Source (patterns)
- src/repl_actions.c (tool execution flow handling)
- src/openai/client.c (response handling/finish_reason)
- src/db/message.c (message persistence)
- src/wrapper.h (mocking patterns for file I/O)
- src/tool.c (file_read error handling implementation)

### Pre-read Tests (patterns)
- tests/integration/message_integration_test.c (integration test patterns with database)
- tests/unit/repl/handle_request_error_test.c (error handling in conversation flow)
- tests/integration/openai_mock_verification_test.c (mock setup patterns for OpenAI responses)
- tests/unit/tool/ (file_read unit tests created by task file-read-execute.md)

## Pre-conditions
- `make check` passes
- All tasks from Stories 01-07 completed
- Task `db-tool-persist.md` completed (tool messages persist to database)
- `ik_tool_exec_file_read()` exists and handles file not found errors
- Tool result messages support error content
- Conversation loop handles tool errors

## Task
Create an end-to-end integration test that verifies file not found errors flow correctly through the entire system:

1. User requests a non-existent file
2. Model responds with file_read tool call
3. Tool execution returns error: `{"error": "File not found: missing.txt"}`
4. Error result is added to conversation as tool message
5. Follow-up request is sent to model with error in tool message
6. Model responds with helpful error explanation
7. All messages persist to database correctly

This test verifies that the error handling implemented in previous tasks (file-read-execute.md, tool-loop-continuation.md) works correctly end-to-end.

**Note**: This is a verification test. The functionality is implemented in earlier tasks. If previous tasks are complete, the test should pass. If it fails, identify and fix gaps in the implementation.

## TDD Cycle

### Red
1. Create `tests/integration/test_file_read_error.c` or extend existing integration tests:
   - Mock OpenAI API to return Response A (file_read tool call for missing.txt)
   - Set up test environment where missing.txt does not exist
   - Verify tool execution is triggered and returns error JSON
   - Verify tool result message contains error
   - Mock OpenAI API to return Response B (helpful error message)
   - Verify Request B includes tool message with error content
   - Verify final response is displayed to user
   - Verify all messages (including error) persist to database
2. Run `make check` - expect test to pass (if error handling is complete) or fail (if gaps exist)

### Green
1. If test fails, identify gaps in error handling:
   - Ensure `ik_tool_exec_file_read()` returns proper error JSON format
   - Ensure tool result messages correctly handle error content
   - Ensure conversation loop doesn't break on tool errors
   - Ensure database persistence handles error messages
2. Fix any identified gaps (should be minimal if previous tasks were complete)
3. Run `make check` - expect pass

### Refactor
1. Ensure test is readable and maintainable
2. Consider extracting common error test patterns for other tools
3. Verify test covers edge cases (different error messages, permission denied, etc.)
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- End-to-end test verifies file not found errors work correctly
- Error flows from tool execution → tool message → model → user
- Database correctly persists conversations with tool errors
- Test can serve as template for error testing other tools
