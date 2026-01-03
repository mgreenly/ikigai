# Task 04: Add Fork Event Persistence

## Model/Thinking

- Model: sonnet
- Thinking: thinking

Moderate complexity - requires understanding fork flow and inserting events at correct points.

## Skills

```
/persona developer
```

## Context

### What

Persist fork events in `cmd_fork()` by inserting:
1. Parent-side fork event before creating child
2. Child-side fork event after child creation

### How

In `src/commands_agent.c` `cmd_fork()`:
1. Before child creation: Insert parent's fork record
2. After child creation: Insert child's fork record

Use the data structures defined in the design document.

### Why

Fork events need to be in the database so:
- Parent's history shows "Forked child RQtxuv..."
- Child's history shows "Forked from wlT4G-..."
- Session replay can reconstruct the fork relationship

## Pre-conditions

- Task 02 completed (fork kind added)
- Task 03 completed (renderers in place or acceptable to error temporarily)
- All tests pass: `make check`

## Post-conditions

- `cmd_fork()` inserts parent-side fork event
- `cmd_fork()` inserts child-side fork event
- Fork events appear in database after forking
- All tests pass: `make check`
- Commit made

## Source Files

- `src/commands_agent.c` - cmd_fork() implementation
- `tests/unit/commands_agent_test.c` or `tests/integration/` - add test

## Fork Event Data Structures

**Parent's history:**
```
kind: "fork"
content: "Forked child RQtxuv..."
data: {"child_uuid": "RQtxuv...", "fork_message_id": 123, "role": "parent"}
```

**Child's history:**
```
kind: "fork"
content: "Forked from wlT4G-..."
data: {"parent_uuid": "wlT4G-...", "fork_message_id": 123, "role": "child"}
```

Properties:
- Same `kind: "fork"` for both
- Different `content` for display
- `data.role` distinguishes parent vs child
- `fork_message_id` links them together

## Implementation Steps

1. Read `src/commands_agent.c` to understand cmd_fork() flow
2. Identify insertion points (before/after child creation)
3. Build JSON data for parent-side event
4. Insert parent-side fork event
5. Build JSON data for child-side event
6. Insert child-side fork event
7. Add test verifying both events are persisted
8. Run `make check`
9. Commit

## Verification

```bash
make check
```

Manual verification:
1. Start app, fork an agent
2. Query database for fork events
3. Verify both parent and child records exist
