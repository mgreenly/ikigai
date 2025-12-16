# Task: Initialize id=0 in Message Creators

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 2 of the message type unification. Now that `ik_msg_t` has an `id` field, all functions that create `ik_msg_t` instances must initialize `id = 0` to indicate these are in-memory messages (not loaded from DB).

**Sentinel value convention:**
- `id = 0`: Message created in-memory (not yet persisted)
- `id > 0`: Valid DB row ID

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- src/msg.h (READ - ik_msg_t struct definition)
- src/openai/client_msg.c (READ - message creator functions)

## Source Files to MODIFY
- src/openai/client_msg.c (MODIFY - initialize id=0 in all ik_msg_t creators)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Prior task completed: gap0-add-id-field.md (ik_msg_t has id field)

## Task

Update all functions in `src/openai/client_msg.c` that create `ik_msg_t` instances to explicitly initialize `id = 0`.

**Functions to modify:**
1. `ik_openai_msg_create()` - around line 24
2. `ik_openai_msg_create_tool_call()` - around line 57
3. `ik_openai_msg_create_tool_result()` - around line 137

**Change pattern:**
```c
// BEFORE (using talloc_zero already sets id to 0, but make it explicit)
ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
...

// AFTER (explicit initialization for clarity)
ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
...
msg->id = 0;  /* In-memory message, not from DB */
```

Note: `talloc_zero` already zeroes all fields, so `id` is technically 0. However, adding explicit `msg->id = 0` makes the intent clear and documents the sentinel value convention. Place this line right after the NULL check for the allocation.

## TDD Cycle

### Red
No new tests needed - this is a clarification change with no behavioral difference.

1. Run `make check` to confirm all tests pass before modification

### Green
1. In `ik_openai_msg_create()`: Add `msg->id = 0;` after NULL check (line ~28)
2. In `ik_openai_msg_create_tool_call()`: Add `msg->id = 0;` after NULL check (line ~59)
3. In `ik_openai_msg_create_tool_result()`: Add `msg->id = 0;` after NULL check (line ~139)
4. Run `make check` - should pass
5. Run `make lint` - should pass

### Refactor
No refactoring needed.

## Sub-agent Usage
- Use haiku sub-agents to run `make check` and `make lint`

## Post-conditions
- `make check` passes
- `make lint` passes
- All `ik_msg_t` creators explicitly set `id = 0`
- Working tree is clean (all changes committed)
