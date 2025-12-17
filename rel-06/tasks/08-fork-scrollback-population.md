# Task 08: Fork Scrollback Population

## Model/Thinking

- Model: sonnet
- Thinking: extended

Complex - requires understanding fork flow, scrollback architecture, and replay mechanism.

## Skills

```
/persona developer
```

## Context

### What

Ensure forked agents have their scrollback populated with parent's history.

### How

Two approaches documented in history.md:

**Option A: DB replay**
- Use `ik_agent_replay_history()` to populate child's scrollback
- Consistent with restart behavior
- Only includes persisted content

**Option B: Copy parent's scrollback**
- Directly copy parent's scrollback buffer to child
- Simpler implementation
- Includes ephemeral content not in DB

### Why

Currently, when forking an agent, the child's scrollback is empty. Users expect the child to inherit the parent's conversation history visually.

## Pre-conditions

- Tasks 01-06 completed (all core fixes, fork events persist)
- Fork events properly recorded in database
- All tests pass: `make check`

## Post-conditions

- Forked agent's scrollback shows parent's history
- Behavior is consistent and predictable
- Tests verify scrollback population
- All tests pass: `make check`
- Commit made

## Source Files

- `src/commands_agent.c` - cmd_fork() implementation
- `src/agent.c` - agent creation
- `src/scrollback.c` - scrollback buffer implementation
- `src/db/agent_replay.c` - if using DB replay approach

## Open Questions

From history.md:
1. Should child's scrollback be via DB replay or by copying parent's buffer?
   - DB replay: Consistent with restart behavior
   - Copy: Simpler, includes ephemeral content

2. What about content rendered but not persisted (pre-fix)?

## Recommended Approach

**Option B (Copy)** is likely simpler:
- Direct memory copy of scrollback lines
- No database queries needed
- Includes all visible content
- Matches user's visual expectation

However, this should be verified against the codebase architecture.

## Implementation Steps

1. Read `src/commands_agent.c` cmd_fork() to understand current flow
2. Read `src/scrollback.c` to understand buffer structure
3. Read `src/agent.c` to understand agent creation
4. Determine if parent's scrollback is accessible at fork time
5. Implement scrollback copy/population
6. Add test verifying child has parent's history
7. Run `make check`
8. Commit

## Verification

```bash
make check
```

Manual verification:
1. Start app, have a conversation
2. Fork the agent
3. Verify child shows parent's history
4. Type in child, verify it continues independently
