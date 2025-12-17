# Task 06: Add command and fork Event Renderers

## Model/Thinking

- Model: sonnet
- Thinking: none

Follow existing pattern - straightforward addition of handlers.

## Skills

```
/persona developer
```

## Context

### What

Add handlers for `command` and `fork` kinds in `ik_event_render()`.

### How

Follow the existing pattern in `src/event_render.c`:
1. Add case for "command" - render the command output
2. Add case for "fork" - render fork message with appropriate styling

### Why

Without renderers, replaying these events will fail with `ERR("Unknown event kind")`, breaking session restore for any session that contains command or fork events.

## Pre-conditions

- Task 03 completed (agent_killed renderer as pattern reference)
- Task 04 completed (fork events exist)
- Task 05 completed (command events exist)
- All tests pass: `make check`

## Post-conditions

- `ik_event_render()` handles "command" kind
- `ik_event_render()` handles "fork" kind
- Rendered output is visually appropriate
- Tests added for both kinds
- All tests pass: `make check`
- Commit made

## Source Files

- `src/event_render.c` - add handlers
- `tests/unit/event_render_test.c` - add tests

## Rendering Guidelines

**command kind:**
- Display the command and its output
- Use a distinct style (perhaps dimmed or system-like)
- Content should show the full output

**fork kind:**
- Parent: "Forked child {uuid}"
- Child: "Forked from {uuid}"
- Use informational styling
- Parse role from data.role to determine display format

## Implementation Steps

1. Read `src/event_render.c` for existing patterns
2. Add "command" handler - render content directly
3. Add "fork" handler - parse data for role, format accordingly
4. Add test cases for both kinds
5. Run `make check`
6. Commit

## Verification

```bash
make check
```
