# Session History Replay Tasks

## Problem

Session history is not restored on app restart or fork. The root cause is that all `ik_db_message_insert()` calls pass `NULL` for `agent_uuid`, so messages are never found by the agent-based replay query.

## Minimal Viable Fix

**Task 01 alone** will restore session history on restart. Other tasks add polish (fork events, slash command persistence).

## Task Dependency Graph

```
Phase 1: Core Fix
  01-fix-agent-uuid-inserts     (ROOT CAUSE FIX)
  02-add-message-kinds          (depends on 01)
  03-add-agent-killed-renderer  (depends on 02)
  04-add-fork-event-persistence (depends on 02, 03)

Phase 2: Slash Command Persistence
  05-persist-slash-command-output (depends on 02)
  06-add-command-fork-renderers   (depends on 03, 04, 05)

Phase 3: Cleanup
  07-cleanup-old-replay         (depends on 01-06)

Phase 4: Fork Improvements
  08-fork-scrollback-population (depends on 01-06)
```

## Task List

| # | Task | Phase | Complexity | Model |
|---|------|-------|------------|-------|
| 01 | Fix agent_uuid in message inserts | 1 | Low | sonnet |
| 02 | Add new message kinds | 1 | Low | sonnet |
| 03 | Add agent_killed renderer | 1 | Low | sonnet |
| 04 | Add fork event persistence | 1 | Medium | sonnet+thinking |
| 05 | Persist slash command output | 2 | Medium | sonnet+thinking |
| 06 | Add command/fork renderers | 2 | Low | sonnet |
| 07 | Cleanup old replay | 3 | Medium | sonnet+thinking |
| 08 | Fork scrollback population | 4 | High | sonnet+extended |

## Execution Order

1. **01** - Fixes root cause, enables session restore
2. **02** - Enables new event types
3. **03** - Prevents replay errors for agent_killed
4. **04** - Records fork events in database
5. **05** - Persists slash command output
6. **06** - Renders new event types
7. **07** - Optional cleanup
8. **08** - Improves fork UX

## Source

Derived from `history.md` - the original investigation and decision document.
