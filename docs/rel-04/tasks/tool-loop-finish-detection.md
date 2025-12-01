# Task: Detect finish_reason and Decide Tool Loop Continuation

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

### Pre-read Source (patterns)
- src/openai/client_multi_callbacks.c (finish_reason extraction pattern)
- src/repl_callbacks.c (streaming callback and completion handling)
- src/repl.h (REPL context structure and assistant_response field)
- src/openai/client_multi.h (completion callback typedef and ik_http_completion_t structure)
- src/openai/sse_parser.c (response parsing and finish_reason detection)

### Pre-read Tests (patterns)
- tests/unit/repl/repl_http_completion_callback_test.c (completion callback testing pattern)
- tests/unit/openai/client_http_sse_streaming_test.c (SSE streaming and callback testing pattern)

## Pre-conditions
- `make check` passes
- Tool call parsing works (`ik_openai_parse_tool_calls`)
- Assistant tool_calls message serialization works
- Task `assistant-tool-calls-msg.md` completed

## Task
Verify and extend existing finish_reason detection for tool loop continuation:

**Note**: The infrastructure already exists:
- `repl->response_finish_reason` stores the finish reason (src/repl.h:75)
- `ik_openai_http_extract_finish_reason()` extracts from SSE events (src/openai/http_handler_internal.h)
- Various structs already have `finish_reason` fields

This task verifies the existing infrastructure works for tool_calls detection and adds any missing accessor if needed:
1. Verify finish_reason is correctly extracted from SSE response with "tool_calls"
2. Verify `repl->response_finish_reason` is populated after response completes
3. Add accessor/check for finish_reason == "tool_calls" if not already present
4. Distinguish between "tool_calls", "stop", and other finish reasons

This task focuses solely on detection and state tracking - it does NOT execute tools, send follow-up requests, or track loop limits (limits are added in Story 11).

## TDD Cycle

### Red
1. Create or extend unit tests for finish_reason detection:
   - Parse response with finish_reason: "tool_calls"
   - Parse response with finish_reason: "stop"
   - Verify finish_reason is stored in context
   - Verify accessor returns correct value
2. Run `make check` - expect test failure (functionality not implemented)

### Green
1. Verify existing `ik_openai_http_extract_finish_reason()` handles "tool_calls" correctly
2. Verify `repl->response_finish_reason` is set after SSE stream completes
3. If not already present, implement accessor (e.g., `ik_repl_should_continue_tool_loop()`)
   - Returns true if `response_finish_reason` equals "tool_calls"
   - Returns false for "stop" or other values
4. Run `make check` - expect pass

### Refactor
1. Ensure naming follows docs/naming.md conventions
2. Consider if finish_reason storage belongs in existing structures
3. Verify error handling for missing/invalid finish_reason
4. Check for duplication with existing SSE parsing logic
5. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- finish_reason is extracted and stored from SSE response
- Accessor function determines if tool loop should continue
- 100% test coverage for new code
