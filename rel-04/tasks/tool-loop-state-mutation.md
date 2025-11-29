# Task: Add Tool Messages to Conversation State

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
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
- rel-04/README.md (tool result format)

### Pre-read Source (patterns)
- src/openai/client.c (conversation management and message handling)
- src/repl.c or src/repl_actions.c (conversation state management)
- src/openai/sse_parser.c (response parsing)
- src/tool.h (tool call and result structures)

### Pre-read Tests (patterns)
- tests/unit/repl/handle_request_success_test.c (conversation loop and request handling patterns)
- tests/unit/repl/repl_http_completion_callback_test.c (API response callback patterns)
- tests/integration/repl_test.c (end-to-end conversation testing)

## Pre-conditions
- `make check` passes
- Tool call parsing works (`ik_openai_parse_tool_calls`)
- Glob execution works (`ik_tool_exec_glob`)
- Tool result message works
- Assistant tool_calls message serialization works
- finish_reason detection works (task `tool-loop-finish-detection.md` completed)

## Task
Implement conversation state mutation to add tool-related messages:
1. Add assistant message with tool_calls to conversation history
2. Execute tool dispatcher to get tool result
3. Add tool result message to conversation history
4. Ensure correct message ordering (assistant -> tool)
5. Handle multiple tool calls in a single response (sequential execution)
6. Preserve conversation state for follow-up request

This task focuses on state mutation only - it does NOT send the follow-up API request.

## TDD Cycle

### Red
1. Create unit tests for conversation state mutation:
   - Start with conversation containing user message
   - Add assistant message with tool_calls
   - Verify assistant message is in conversation
   - Execute glob tool (mock or real)
   - Add tool result message
   - Verify tool result message is in conversation
   - Verify message ordering is correct
   - Test with multiple tool calls (sequential)
2. Run `make check` - expect test failure (mutation not implemented)

### Green
1. Implement function to add assistant tool_calls message to conversation
2. Integrate tool dispatcher call (use existing `ik_tool_exec_glob`)
3. Implement function to add tool result message to conversation
4. Ensure proper ordering: user -> assistant(tool_calls) -> tool
5. Handle multiple tool calls sequentially
6. Preserve all conversation state
7. Run `make check` - expect pass

### Refactor
1. Consider extracting tool dispatch logic for reusability
2. Ensure memory ownership is clear (see docs/memory.md)
3. Check error handling for tool execution failures
4. Verify naming consistency with existing conversation functions
5. Consider if tool result error format matches rel-04/README.md
6. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Assistant tool_calls message is added to conversation
- Tool is executed via dispatcher
- Tool result message is added to conversation
- Message ordering is preserved
- Multiple tool calls are handled sequentially
- Conversation state is ready for follow-up request
- 100% test coverage for new code
