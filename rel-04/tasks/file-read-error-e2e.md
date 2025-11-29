# Task: End-to-End Test for File Read Error Handling

## Target
User story: 08-file-not-found-error

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- All tasks from Stories 01-07 completed
- `ik_tool_exec_file_read()` exists and handles file not found errors
- Tool result messages support error content
- Conversation loop handles tool errors

## Context
Read before starting:
- rel-04/user-stories/08-file-not-found-error.md (full walkthrough)
- src/tool.c (file_read error handling implementation)
- tests/unit/tool/test_file_read_execute.c (unit tests for error cases)
- tests/integration/ (existing integration test patterns)

## Task
Create an end-to-end integration test that verifies file not found errors flow correctly through the entire system:

1. User requests a non-existent file
2. Model responds with file_read tool call
3. Tool execution returns error: `{"error": "File not found: missing.txt"}`
4. Error result is added to conversation as tool message
5. Follow-up request is sent to model with error in tool message
6. Model responds with helpful error explanation
7. All messages persist to database correctly

This test verifies that the error handling implemented in previous tasks (file-read-execute.md, conversation-tool-loop.md) works correctly end-to-end.

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
