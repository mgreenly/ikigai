# Task: Parse Tool Calls from SSE Response

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists in `src/tool.h`
- Task `tool-call-struct.md` completed

## Context
Read before starting:
- src/openai/sse_parser.h (existing SSE parsing)
- src/openai/sse_parser.c (implementation patterns)
- src/tool.h (ik_tool_call_t struct)
- rel-04/user-stories/02-single-glob-call.md (see Response A for tool_calls format)

## Task
Extend SSE parsing to detect and extract tool_calls from response deltas. The parser must:
1. Detect `finish_reason: "tool_calls"`
2. Extract tool_calls array from delta
3. Return parsed `ik_tool_call_t` structures

Note: Tool call arguments stream in chunks across multiple deltas. For Story 02, we handle single complete tool calls (arguments arrive in one chunk). Streaming accumulation is out of scope.

## TDD Cycle

### Red
1. Create `tests/unit/openai/test_sse_tool_calls.c`
2. Write tests:
   - Parse delta with tool_calls returns non-NULL result
   - Extracts tool call id correctly
   - Extracts function name correctly
   - Extracts arguments correctly
   - Returns NULL for delta with only content (no tool_calls)
   - Handles finish_reason: "tool_calls" detection
3. Run `make check` - expect compile failure

### Green
1. Add to `src/openai/sse_parser.h`:
   - Include tool.h
   - Declare `ik_openai_parse_tool_calls(void *parent, const char *event)` returning `res_t`
   - Result contains array of `ik_tool_call_t*` or NULL if no tool calls
2. Implement in `src/openai/sse_parser.c`:
   - Check for tool_calls in delta object
   - Extract id, function.name, function.arguments
   - Create ik_tool_call_t for each tool call
   - Return array (or single pointer for Story 02 scope)
3. Run `make check` - expect pass

### Refactor
1. Consider if result type should be explicit struct vs array
2. Ensure error handling follows existing patterns
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_openai_parse_tool_calls()` extracts tool calls from SSE events
- 100% test coverage for new code
