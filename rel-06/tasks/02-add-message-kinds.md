# Task 02: Add New Message Kinds

## Model/Thinking

- Model: sonnet
- Thinking: none

Simple addition to a validation array.

## Skills

```
/persona developer
```

## Context

### What

Add `command` and `fork` to the list of valid message kinds in `ik_db_message_is_valid_kind()`.

### How

Update the `VALID_KINDS` array in `src/db/message.c` to include the new kinds.

### Why

These new kinds are needed for:
- `command`: Persisting slash command output so it survives restart
- `fork`: Recording fork events in both parent and child histories

Without these, `ik_db_message_insert()` will reject messages with these kinds.

## Pre-conditions

- Task 01 completed (agent_uuid fix)
- All tests pass: `make check`

## Post-conditions

- `ik_db_message_is_valid_kind("command")` returns true
- `ik_db_message_is_valid_kind("fork")` returns true
- Tests added for new kinds
- All tests pass: `make check`
- Commit made

## Source Files

- `src/db/message.c` - VALID_KINDS array
- `src/db/message.h` - documentation of valid kinds
- `tests/unit/db/message_test.c` - add tests for new kinds

## Current Valid Kinds

```c
clear, system, user, assistant, tool_call, tool_result, mark, rewind, agent_killed
```

## Implementation Steps

1. Read `src/db/message.c` to find VALID_KINDS
2. Add "command" and "fork" to the array
3. Update header documentation if present
4. Add test cases in `tests/unit/db/message_test.c`
5. Run `make check`
6. Commit

## Verification

```bash
make check
```
