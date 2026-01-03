# Task 01: Fix agent_uuid in Message Inserts

## Model/Thinking

- Model: sonnet
- Thinking: none

This is a straightforward fix - passing existing value instead of NULL at known call sites.

## Skills

```
/persona developer
```

## Context

### What

Update all `ik_db_message_insert()` calls to pass `repl->current->uuid` instead of `NULL`. This is the root cause of session history not being restored on restart.

### How

Each call site needs to:
1. Access `repl->current->uuid` (already available in scope via `repl` parameter)
2. Pass it as the `agent_uuid` parameter instead of `NULL`

### Why

Messages inserted with `agent_uuid = NULL` are never found by the agent-based replay query because `NULL = 'any-value'` is always false in SQL. The restoration code (`ik_repl_restore_agents()`) uses agent-based replay, so no messages are found on restart.

## Pre-conditions

- All tests pass: `make check`
- The call sites exist as documented

## Post-conditions

- All `ik_db_message_insert()` calls pass `repl->current->uuid`
- All tests pass: `make check`
- Commit made with descriptive message

## Call Sites to Fix

| File | Line | Message Kind | Current | Fix |
|------|------|--------------|---------|-----|
| `src/repl_actions_llm.c` | 103-104 | user | NULL | repl->current->uuid |
| `src/repl_event_handlers.c` | 156-157 | assistant | NULL | repl->current->uuid |
| `src/repl_tool.c` | 115-118 | tool_call | NULL | repl->current->uuid |
| `src/repl_tool.c` | 115-118 | tool_result | NULL | repl->current->uuid |
| `src/repl_tool.c` | 255-258 | tool_call (async) | NULL | repl->current->uuid |
| `src/repl_tool.c` | 255-258 | tool_result (async) | NULL | repl->current->uuid |
| `src/commands.c` | 203-204 | clear | NULL | repl->current->uuid |
| `src/commands.c` | 219-221 | system | NULL | repl->current->uuid |
| `src/commands_mark.c` | 96-97 | mark | NULL | repl->current->uuid |
| `src/commands_mark.c` | 156-157 | rewind | NULL | repl->current->uuid |
| `src/commands_agent.c` | 95-96 | user (fork prompt) | NULL | repl->current->uuid |

## Files Already Correct (No Changes Needed)

- `src/repl/agent_restore.c` lines 144-147, 165-168 - already pass `repl->current->uuid`
- `src/commands_agent.c` lines 209, 290, 394 - `agent_killed` inserts already correct

## Implementation Steps

1. Read each file to confirm line numbers
2. Update each call site to pass `repl->current->uuid`
3. Run `make check` after each file modification
4. Commit after all changes pass

## Verification

```bash
make check
```

Session history should now be queryable by agent_uuid.
