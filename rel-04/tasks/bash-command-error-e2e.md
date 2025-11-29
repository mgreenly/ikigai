# Task: End-to-End Test for Bash Command Error Handling

## Target
User story: 09-bash-command-fails

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- All tasks from Stories 01-08 completed
- `ik_tool_exec_bash()` exists and handles command failures
- Tool result messages support non-zero exit codes
- Conversation loop handles tool results with failed commands

## Context
Read before starting:
- rel-04/user-stories/09-bash-command-fails.md (full walkthrough)
- src/tool.c (bash error handling implementation)
- tests/unit/tool/test_bash_execute.c (unit tests for error cases)
- rel-04/tasks/file-read-error-e2e.md (similar E2E test pattern)
- tests/integration/ (existing integration test patterns)

## Task
Create an end-to-end integration test that verifies bash command failures flow correctly through the entire system:

1. User requests to run a command that will fail
2. Model responds with bash tool call
3. Tool execution returns error output with non-zero exit code: `{"output": "...error message...", "exit_code": 1}`
4. Error result is added to conversation as tool message
5. Follow-up request is sent to model with error output in tool message
6. Model responds with helpful explanation of the failure
7. All messages persist to database correctly

This test verifies that the error handling implemented in bash-execute.md works correctly end-to-end.

## TDD Cycle

### Red
1. Create `tests/integration/test_bash_command_error.c` or extend existing integration tests:
   - Mock OpenAI API to return Response A (bash tool call for "gcc main.c")
   - Set up test environment where gcc will fail (missing object files, etc.)
   - Verify tool execution is triggered and returns error JSON with non-zero exit code
   - Verify tool result message contains error output and exit code
   - Mock OpenAI API to return Response B (helpful error explanation)
   - Verify Request B includes tool message with error output
   - Verify final response is displayed to user
   - Verify all messages (including error) persist to database
2. Run `make check` - expect test to pass (if error handling is complete) or fail (if gaps exist)

### Green
1. If test fails, identify gaps in error handling:
   - Ensure `ik_tool_exec_bash()` returns proper error format with non-zero exit code
   - Ensure tool result messages correctly handle error output
   - Ensure conversation loop doesn't break on bash tool errors
   - Ensure database persistence handles failed command messages
2. Fix any identified gaps (should be minimal if bash-execute.md was complete)
3. Run `make check` - expect pass

### Refactor
1. Ensure test is readable and maintainable
2. Consider extracting common error test patterns for other tools
3. Verify test covers edge cases (different error messages, permission denied, etc.)
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- End-to-end test verifies bash command failures work correctly
- Error output flows from tool execution → tool message → model → user
- Database correctly persists conversations with failed bash commands
- Test can serve as template for error testing other tools
