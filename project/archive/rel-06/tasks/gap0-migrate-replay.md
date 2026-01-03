# Task: Migrate db/replay to ik_msg_t

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 3 of the message type unification - the core migration. We change `ik_replay_context_t.messages` from `ik_message_t**` to `ik_msg_t**`.

**Why this works now:**
- `ik_msg_t` now has an `id` field (identical to `ik_message_t`)
- Both types have the same fields: `id`, `kind`, `content`, `data_json`
- We can use `ik_msg_t` everywhere `ik_message_t` was used

**What changes:**
- `ik_replay_context_t.messages` type changes from `ik_message_t**` to `ik_msg_t**`
- Internal functions in replay.c use `ik_msg_t` instead of `ik_message_t`
- The `ik_message_t` typedef is kept temporarily for backward compatibility (removed in later task)

**Impact:**
- session_restore.c will need updates (later task)
- agent_replay.c will need updates (later task)
- Tests will need updates (later tasks)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section, specifically the type unification steps)

## Pre-read Source
- src/msg.h (READ - ik_msg_t definition with id field)
- src/db/replay.h (READ - ik_replay_context_t, ik_message_t definitions)
- src/db/replay.c (READ - replay implementation using ik_message_t)

## Source Files to MODIFY
- src/db/replay.h (MODIFY - change ik_replay_context_t.messages type)
- src/db/replay.c (MODIFY - use ik_msg_t throughout)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Prior tasks completed:
  - gap0-add-id-field.md (ik_msg_t has id field)
  - gap0-init-id-creators.md (creators initialize id=0)

## Task

### Part 1: Update replay.h

1. Add `#include "../msg.h"` to get `ik_msg_t` definition

2. Change `ik_replay_context_t.messages` from `ik_message_t**` to `ik_msg_t**`:
```c
// BEFORE (line 44-49)
typedef struct {
    ik_message_t **messages;          // Dynamic array of message pointers
    ...
} ik_replay_context_t;

// AFTER
typedef struct {
    ik_msg_t **messages;              // Dynamic array of message pointers (unified type)
    ...
} ik_replay_context_t;
```

3. Keep the `ik_message_t` typedef for now (it will be removed in a later task)

### Part 2: Update replay.c

Change all internal usage of `ik_message_t` to `ik_msg_t`:

1. **ensure_capacity()** (line 40-41):
```c
// BEFORE
ik_message_t **new_messages = talloc_realloc(..., ik_message_t *, ...);

// AFTER
ik_msg_t **new_messages = talloc_realloc(..., ik_msg_t *, ...);
```

2. **append_message()** (line 101):
```c
// BEFORE
ik_message_t *msg = talloc_zero(context, ik_message_t);

// AFTER
ik_msg_t *msg = talloc_zero(context, ik_msg_t);
```

**Search pattern:** Find all `ik_message_t` in replay.c and change to `ik_msg_t`.

## TDD Cycle

### Red
The build may break temporarily due to type mismatches in dependent code. This is expected.

1. Run `make` to see initial state

### Green
1. Update replay.h:
   - Add `#include "../msg.h"`
   - Change `ik_message_t **messages` to `ik_msg_t **messages` in ik_replay_context_t

2. Update replay.c:
   - Change all `ik_message_t` to `ik_msg_t` (approximately 3-4 occurrences)

3. Run `make` - may have warnings about type mismatches in other files (expected)
4. Run `make check` - some tests may fail due to type changes (expected, fixed in later tasks)

**Note:** If tests fail, that's expected. This task establishes the new type system. Later tasks update the consumers.

### Refactor
No refactoring needed at this stage.

## Sub-agent Usage
- Use haiku sub-agents to run `make` to check compilation
- You may use sub-agents to search for all `ik_message_t` occurrences in replay.c

## Post-conditions
- Code compiles (`make` succeeds)
- `make lint` passes
- `ik_replay_context_t.messages` is now `ik_msg_t**`
- replay.c uses `ik_msg_t` internally
- Working tree is clean (all changes committed)

## Note on Test Failures

Some tests may fail after this change because:
- session_restore tests create mock `ik_message_t` objects
- agent_replay tests use `ik_message_t`

These are fixed in subsequent tasks. Commit this change even if some tests fail - the type migration must be done atomically within replay.c/h.
