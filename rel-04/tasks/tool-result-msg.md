# Task: Create Tool Result Message

## Target
User story: 02-single-glob-call

## Agent
model: haiku

## Pre-conditions
- `make check` passes
- `ik_tool_exec_glob()` exists and returns JSON result
- Task `glob-execute.md` completed

## Context
Read before starting:
- src/openai/client.h (ik_openai_msg_t structure)
- src/openai/client.c (message handling)
- rel-04/user-stories/02-single-glob-call.md (see Request B for tool message format)

## Task
Create a tool result message that can be added to conversation for Request B. The message format from the user story:

```json
{"role": "tool", "tool_call_id": "call_abc123", "content": "{\"output\": \"...\", \"count\": 3}"}
```

This requires extending or creating a new message type since `ik_openai_msg_t` only has role/content.

## TDD Cycle

### Red
1. Add tests in `tests/unit/openai/test_client.c` or new file:
   - `ik_openai_tool_msg_create()` returns non-NULL
   - Message has role "tool"
   - Message has tool_call_id field
   - Message has content field with result JSON
2. Run `make check` - expect compile failure

### Green
1. Add to `src/openai/client.h`:
   - Either extend `ik_openai_msg_t` with optional `tool_call_id` field
   - Or create `ik_openai_tool_msg_t` struct
   - Declare `ik_openai_tool_msg_create(void *parent, const char *tool_call_id, const char *content)`
2. Implement in `src/openai/client.c`:
   - Allocate message struct
   - Set role to "tool"
   - Copy tool_call_id and content
3. Run `make check` - expect pass

### Refactor
1. Consider if tool message should share base with regular message
2. Ensure consistent memory ownership patterns
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Tool result message can be created with role, tool_call_id, content
- 100% test coverage for new code
