# Task: Add Tool Messages to Conversation State

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
- rel-04/README.md (sections: "Canonical Message Format", "Tool Result Format")

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
- Canonical message format defined (`ik_msg_t` with `kind` discriminator)
- Tool result canonical message creation works (kind="tool_result")
- Assistant tool_calls canonical message creation works (kind="tool_call")
- OpenAI serialization converts canonical messages to OpenAI format for API requests
- finish_reason detection works (task `tool-loop-finish-detection.md` completed)

## Task
Implement conversation state mutation to add tool-related messages in canonical format:
1. Add assistant message with tool_calls to conversation history as canonical message:
   - kind="tool_call"
   - content=human-readable description
   - data_json=tool call data (JSON string)
2. Execute tool dispatcher to get tool result
3. Add tool result message to conversation history as canonical message:
   - kind="tool_result"
   - content=summary/human-readable result
   - data_json=structured result data (JSON string)
4. Ensure correct message ordering (user -> assistant/tool_call -> tool_result)
5. Handle multiple tool calls in a single response (sequential execution)
6. Preserve conversation state for follow-up request

This task focuses on state mutation only - it does NOT send the follow-up API request.
The conversation in memory uses canonical format; OpenAI serialization happens when making API requests.

## TDD Cycle

### Red
1. Create unit tests for conversation state mutation using canonical message format:
   - Start with conversation containing user message (kind="user")
   - Add assistant message with tool_calls as canonical message (kind="tool_call")
   - Verify canonical message is in conversation with correct kind, content, and data_json
   - Execute glob tool (mock or real)
   - Add tool result as canonical message (kind="tool_result")
   - Verify canonical tool result message is in conversation with correct kind, content, and data_json
   - Verify message ordering is correct (user -> tool_call -> tool_result)
   - Test with multiple tool calls (sequential)
   - Verify NO OpenAI-specific message format (role="assistant", role="tool") in conversation
2. Run `make check` - expect test failure (mutation not implemented)

### Green
1. Implement function to add assistant tool_calls message to conversation as canonical message:
   - Create `ik_msg_t` with kind="tool_call"
   - Set content to human-readable description
   - Set data_json to serialized tool call data
   - Add to conversation
2. Integrate tool dispatcher call (use existing `ik_tool_exec_glob`)
3. Implement function to add tool result message to conversation as canonical message:
   - Create `ik_msg_t` with kind="tool_result"
   - Set content to human-readable summary
   - Set data_json to serialized result data
   - Add to conversation
4. Ensure proper ordering: user -> tool_call -> tool_result
5. Handle multiple tool calls sequentially (each gets tool_call + tool_result pair)
6. Preserve all conversation state (no OpenAI-specific format in memory)
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
- Assistant tool_calls message is added to conversation as canonical message (kind="tool_call")
- Tool is executed via dispatcher
- Tool result message is added to conversation as canonical message (kind="tool_result")
- Message ordering is preserved (user -> tool_call -> tool_result)
- Multiple tool calls are handled sequentially (each gets tool_call + tool_result pair)
- Conversation uses canonical format (ik_msg_t), NOT OpenAI-specific format
- Conversation state is ready for follow-up request (OpenAI serialization happens during API call)
- 100% test coverage for new code
