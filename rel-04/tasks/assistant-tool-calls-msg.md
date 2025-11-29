# Task: Serialize Assistant Message with Tool Calls

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/testability.md
- .agents/skills/coverage.md
- .agents/skills/quality.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/memory.md
- docs/naming.md
- docs/error_patterns.md
- docs/LCOV_EXCLUSIONS.md
- rel-04/user-stories/02-single-glob-call.md (user story - see Request B for assistant message format)

### Pre-read Source (patterns)
- src/openai/client.h (extend ik_openai_msg_t struct definition)
- src/openai/client.c (ik_openai_serialize_request with yyjson mutation patterns and message iteration)

### Pre-read Tests (patterns)
- tests/unit/openai/client_structures_test.c (serialization test patterns, yyjson value parsing assertions, talloc context setup)

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists
- Tool result message can be created
- Task `tool-result-msg.md` completed

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
2. Extend `src/openai/client.h`:
   - Add tool_calls field to message struct (or create variant)
   - Add declaration for function to create assistant message with tool_calls
3. Add stub in `src/openai/client.c`:
   - Function creates message but sets tool_calls to NULL
   - Serialization ignores tool_calls (outputs "content" only)
4. Run `make check` - expect assertion failure (tests expect "tool_calls" array in serialized JSON)

### Green
1. Replace stub in `src/openai/client.c` with implementation:
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
