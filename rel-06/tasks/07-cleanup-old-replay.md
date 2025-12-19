# Task 07: Cleanup Old Replay Mechanism

## Model/Thinking

- Model: sonnet
- Thinking: thinking

Moderate complexity - decision required on migration vs retention.

## Skills

```
/persona developer
```

## Context

### What

Decide on and implement cleanup of the old `ik_db_messages_load()` replay mechanism.

### How

Two options:

**Option A: Migrate tests to agent-based replay**
- Update 15+ test files to use `ik_agent_replay_history()`
- Remove `ik_db_messages_load()` from `src/db/replay.c`
- Remove from header and any references

**Option B: Keep for test convenience**
- Leave `ik_db_messages_load()` in place
- Document that it's test-only (no production use)
- Simpler, no impact on production

### Why

The old mechanism queries by `session_id` only, not `agent_uuid`. It has no production callers (only tests use it). Keeping it for test convenience is simpler and has no production impact.

## Pre-conditions

- Tasks 01-06 completed (all core fixes)
- All tests pass: `make check`

## Post-conditions

If Option A:
- All tests migrated to `ik_agent_replay_history()`
- `ik_db_messages_load()` removed
- All tests pass: `make check`

If Option B:
- `ik_db_messages_load()` documented as test-only
- All tests pass: `make check`

Commit made with decision documented.

## Source Files

- `src/db/replay.c` - old mechanism
- `src/db/replay.h` - header
- `tests/unit/db/replay_test.c` - direct tests
- Multiple test files using `ik_db_messages_load()`

## Decision Recommendation

**Option B (Keep for tests)** is recommended:
- Lower risk - no test migration needed
- Faster to complete
- Old mechanism works fine for test purposes
- Production code already uses agent-based replay

## Implementation Steps

### If Option B (Recommended):

1. Add comment to `ik_db_messages_load()` documenting test-only use
2. Add note to header file
3. Commit with explanation

### If Option A:

1. Find all test files using `ik_db_messages_load()`
2. Update each to use `ik_agent_replay_history()`
3. May require test refactoring for agent-based queries
4. Remove function from replay.c
5. Remove from replay.h
6. Run `make check`
7. Commit

## Verification

```bash
make check
```
