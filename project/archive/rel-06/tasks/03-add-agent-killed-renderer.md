# Task 03: Add agent_killed Event Renderer

## Model/Thinking

- Model: sonnet
- Thinking: none

Follow existing pattern in event_render.c for new kind handler.

## Skills

```
/persona developer
```

## Context

### What

Add a handler for `agent_killed` events in `ik_event_render()`. Currently, replaying an `agent_killed` event causes `ERR("Unknown event kind")`.

### How

Follow the pattern of existing handlers in `src/event_render.c`:
1. Add a case for "agent_killed" in the kind dispatch
2. Render appropriate content (e.g., "Agent {name} killed")
3. Use suitable styling (similar to system messages or a new style)

### Why

The `agent_killed` kind is persisted by `/kill` commands but has no renderer. During session replay, these events will fail, breaking the entire replay. This must be fixed for session restore to work correctly.

## Pre-conditions

- Task 02 completed (message kinds)
- All tests pass: `make check`

## Post-conditions

- `ik_event_render()` handles "agent_killed" without error
- Rendered output shows meaningful information about the killed agent
- Tests added for agent_killed rendering
- All tests pass: `make check`
- Commit made

## Source Files

- `src/event_render.c` - main render function
- `src/event_render.h` - header (if documentation needed)
- `tests/unit/event_render_test.c` - add test for agent_killed

## Reference

Look at existing handlers in event_render.c for patterns:
- `user`, `assistant`, `system` - conversation messages
- `mark`, `rewind` - control events
- `clear` - session events

The `agent_killed` event data likely contains:
- Agent name/UUID being killed
- Parent UUID (if relevant)

## Implementation Steps

1. Read `src/event_render.c` to understand dispatch pattern
2. Read existing `agent_killed` insert code to understand data format
3. Add handler for "agent_killed" kind
4. Add test case
5. Run `make check`
6. Commit

## Verification

```bash
make check
```
