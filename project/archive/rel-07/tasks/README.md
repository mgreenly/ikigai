# Revised Task Sequence - Simpler Approach

## Overview

This directory contains the **revised task sequence** using the simpler "rip and replace" approach instead of the dual-mode migration strategy.

## Key Difference from Original Tasks

**Original approach (scratch/tasks/):**
- Maintain BOTH old and new storage simultaneously for 4+ tasks
- Populate both in parallel ("dual mode")
- Old API remains authoritative during migration
- Switch to new API in task 5
- Complex synchronization and potential hidden bugs

**New approach (scratch/tasks-revised/):**
- Create new API
- Delete old storage immediately
- Fix all compilation errors with new API
- Tests must pass after EACH task
- Simpler, clearer failure modes

## Task Sequence

1. **01-create-message-api.md** - Create new message API, replace agent->conversation with agent->messages
2. **02-migrate-repl-to-new-api.md** - Update REPL to use new API, remove all conversation calls
3. **03-migrate-providers-to-new-storage.md** - Update providers to read from new storage
4. **04-migrate-restore-to-new-api.md** - Update restore to populate new storage
5. **05-delete-legacy-openai-code.md** - Delete src/openai/, shim layer, refactor to direct JSON
6. **06-verify-migration-complete.md** - Comprehensive verification

## Key Principles

### Tests Must Pass After Each Task

Every task ends with:
```bash
make clean
make all
make check  # Must pass
```

If tests fail, the task is not complete. Fix them before moving to next task.

### No Dual Mode

Agent struct has ONLY new fields:
```c
// Good - new approach
ik_message_t **messages;
size_t message_count;
size_t message_capacity;
```

NOT this (dual mode):
```c
// Bad - old dual-mode approach
ik_openai_conversation_t *conversation;  // Old
ik_message_t **messages;                 // New
```

### Clear Entry Points

REPL boundaries:
- **REPL creates messages:** `ik_message_create_text/tool_call/tool_result`
- **REPL stores messages:** `ik_agent_add_message`
- **Providers read messages:** `agent->messages[i]`
- **Providers build requests:** `ik_request_build_from_conversation`

No REPL code calls OpenAI-specific functions. All provider interaction through provider interface.

### System Message Handling

System messages are NOT in the message array:
- Old way: System message in conversation as first message
- New way: System prompt in `agent->system_prompt` string field
- Request builder: Calls `ik_request_set_system(req, agent->system_prompt)`

When converting from database:
- `ik_message_from_db_msg()` returns NULL for system messages
- This is intentional - they're handled separately

### Acceptable to Break Temporarily

Task 1 may break old tests - that's OK:
- Agent struct changed (removed conversation field)
- Old tests reference agent->conversation
- Task 2 fixes them by updating to agent->messages

The requirement is: **make check passes at END of each task**, not during.

## Execution Strategy

For unattended execution:
1. Read task file completely
2. Execute all steps in order
3. Run verification commands
4. Only proceed if `make check` passes
5. If it fails, debug and fix before moving to next task

For debugging failures:
1. Read compilation errors carefully
2. Search for references to deleted code
3. Replace with new API calls
4. Recompile and test
5. Repeat until clean

## Comparison with Original Tasks

| Aspect | Original (scratch/tasks/) | Revised (scratch/tasks-revised/) |
|--------|---------------------------|----------------------------------|
| Dual mode | Yes (4+ tasks) | No |
| Complexity | High (sync two systems) | Low (one system at a time) |
| Hidden bugs | Possible (old API masks new bugs) | Immediate (breaks force fixes) |
| Test requirement | Existing tests pass | All tests pass (after fixes) |
| Agent struct | Both fields during migration | Only new fields |
| Debugging | Hard (which storage is wrong?) | Easy (one source of truth) |
| Task count | 9 tasks | 6 tasks |
| Total changes | Same | Same |
| Risk profile | Hidden failures later | Loud failures early |

## Success Criteria (Final Task)

Migration complete when:
1. `src/openai/` directory deleted
2. Shim layer deleted
3. Agent struct has only `messages` field
4. No code outside `src/providers/openai/` calls legacy functions
5. All three providers (Anthropic, OpenAI, Google) work
6. `make check` passes
7. No grep finds legacy references

## Why This Approach is Better

**Clearer failure modes:**
- Compilation errors point directly to what needs fixing
- Test failures show exactly what broke
- No "it works with old API but fails with new API" mystery bugs

**Simpler mental model:**
- One storage system at a time
- No synchronization logic
- No "which is authoritative" questions

**Faster iteration:**
- Fix breaks immediately
- No complex dual-mode code to maintain
- Easier to debug

**Same end result:**
- Both approaches end at same place
- Both delete same code
- Both add same new code
- This path is just straighter
