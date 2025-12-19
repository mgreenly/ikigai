# Task 05: Persist Slash Command Output

## Model/Thinking

- Model: sonnet
- Thinking: thinking

Moderate complexity - requires identifying dispatch point and consistent persistence.

## Skills

```
/persona developer
```

## Context

### What

Persist slash command output to the database so it survives restart. Currently, slash command output (`/agents`, `/help`, `/model`, etc.) is rendered directly to scrollback but NOT persisted.

### How

Options:
1. **Centralized**: Persist in `ik_cmd_dispatch()` after command execution
2. **Distributed**: Persist in each command handler

Centralized is preferred for consistency.

### Why

Without persistence, slash command output is ephemeral - lost on restart even when the agent_uuid fix is applied. Users expect to see their command history after restarting.

## Pre-conditions

- Task 02 completed (command kind added)
- All tests pass: `make check`

## Post-conditions

- Slash command output is persisted with kind="command"
- Output includes command name and result
- Replayed command events appear in scrollback
- All tests pass: `make check`
- Commit made

## Source Files

- `src/commands.c` - ik_cmd_dispatch() and command handlers
- `tests/unit/commands_test.c` - add persistence tests

## Design Decisions Needed

1. **What to persist**: Full output vs summary?
2. **Data format**: `content` = rendered output, `data` = command metadata?
3. **Which commands**: All slash commands or only some?

Suggested format:
```
kind: "command"
content: "/help\n[rendered help output]"
data: {"command": "help", "args": null}
```

## Implementation Steps

1. Read `src/commands.c` to understand dispatch flow
2. Identify where command output is generated
3. Add persistence call after successful command execution
4. Include command name and output in persisted message
5. Add test verifying command output is persisted
6. Run `make check`
7. Commit

## Verification

```bash
make check
```

Manual verification:
1. Start app, run `/help`
2. Query database for command events
3. Verify help output is stored
