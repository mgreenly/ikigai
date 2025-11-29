# Task: Serialize Assistant Message with Tool Calls

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists
- Tool result message can be created
- Task `tool-result-msg.md` completed

## Context
Read before starting:
- src/openai/client.h (message structures)
- src/openai/client.c (serialization with yyjson)
- rel-04/user-stories/02-single-glob-call.md (see Request B for assistant message format)

## Task
Enable serialization of assistant messages that contain tool_calls (no content). Request B shows:

```json
{"role": "assistant", "tool_calls": [{"id": "call_abc123", "type": "function", "function": {"name": "glob", "arguments": "{\"pattern\": \"*.c\", \"path\": \"src/\"}"}}]}
```

This requires:
1. Extending `ik_openai_msg_t` or creating variant for assistant+tool_calls
2. Updating serialization to output tool_calls array instead of content

## TDD Cycle

### Red
1. Add tests in `tests/unit/openai/test_client.c`:
   - Create assistant message with tool_calls
   - Serialize request containing this message
   - Verify JSON has "role": "assistant"
   - Verify JSON has "tool_calls" array (not "content")
   - Verify tool_calls[0] has id, type, function.name, function.arguments
2. Run `make check` - expect compile failure or test failure

### Green
1. Extend message handling in `src/openai/client.h`:
   - Add tool_calls field to message struct (or create variant)
   - Add function to create assistant message with tool_calls
2. Update serialization in `src/openai/client.c`:
   - Detect if message has tool_calls
   - Serialize tool_calls array instead of content when present
3. Run `make check` - expect pass

### Refactor
1. Ensure clean separation between content messages and tool_calls messages
2. Verify serialization matches exact format from user story
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Assistant messages with tool_calls serialize correctly for Request B
- 100% test coverage for new code
