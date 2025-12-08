# Task: Serialize Assistant Message with Tool Calls

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
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
- rel-04/README.md (section "Canonical Message Format" - tool_call message structure)
- rel-04/user-stories/02-single-glob-call.md (user story - see Request B for assistant message format)

### Pre-read Source (patterns)
- src/openai/client.h (ik_msg_t canonical message structure)
- src/openai/client.c (ik_openai_serialize_request with yyjson mutation patterns and message iteration)

### Pre-read Tests (patterns)
- tests/unit/openai/client_structures_test.c (serialization test patterns, yyjson value parsing assertions, talloc context setup)

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists
- Tool result message can be created
- Task `tool-result-msg.md` completed

## Task
Enable creation and serialization of canonical tool_call messages. The canonical message uses:

```c
ik_msg_t {
    kind: "tool_call"
    content: "glob(pattern=\"*.c\", path=\"src/\")"  // human-readable summary
    data_json: "{\"id\": \"call_abc123\", \"type\": \"function\", \"function\": {\"name\": \"glob\", \"arguments\": \"{\\\"pattern\\\": \\\"*.c\\\", \\\"path\\\": \\\"src/\\\"}\"}}"
}
```

The OpenAI serializer (`ik_openai_serialize_request`) transforms this canonical format to the OpenAI wire format shown in Request B:

```json
{"role": "assistant", "tool_calls": [{"id": "call_abc123", "type": "function", "function": {"name": "glob", "arguments": "{\"pattern\": \"*.c\", \"path\": \"src/\"}"}}]}
```

This requires:
1. Creating canonical messages with kind="tool_call", content (human-readable summary), and data_json (structured tool call data)
2. Updating serialization to detect kind="tool_call" and output OpenAI's role="assistant" + tool_calls array format

## TDD Cycle

### Red
1. Add tests in `tests/unit/openai/test_client.c`:
   - Create canonical message with kind="tool_call", content (human-readable summary), and data_json (structured tool call data)
   - Serialize request containing this canonical tool_call message
   - Verify JSON has "role": "assistant" (not "tool_call")
   - Verify JSON has "tool_calls" array (not "content")
   - Verify tool_calls[0] has id, type, function.name, function.arguments matching data_json
2. Extend `src/openai/client.h`:
   - Add declaration for function to create canonical tool_call message (takes id, type, function name/args)
   - Function should populate kind="tool_call", content (human-readable), and data_json (structured)
3. Add stub in `src/openai/client.c`:
   - Function creates message with kind="tool_call" but empty data_json
   - Serialization detects kind="tool_call" but outputs empty tool_calls array
4. Run `make check` - expect assertion failure (tests expect tool_calls[0] with correct structure)

### Green
1. Replace stub in `src/openai/client.c` with implementation:
   - Function properly populates data_json with {id, type, function: {name, arguments}}
   - Serialization parses data_json and transforms to OpenAI wire format
   - When kind="tool_call", output role="assistant" + tool_calls array (no content field)
2. Run `make check` - expect pass

### Refactor
1. Ensure clean transformation from canonical format to OpenAI wire format
2. Verify serialization matches exact format from user story Request B
3. Confirm that canonical message format is provider-agnostic (no OpenAI-specific fields in ik_msg_t)
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Canonical tool_call messages can be created with kind="tool_call", content, and data_json
- OpenAI serializer correctly transforms canonical tool_call messages to OpenAI wire format (Request B)
- Serialized JSON has role="assistant" + tool_calls array (no content field for tool_call messages)
- 100% test coverage for new code
