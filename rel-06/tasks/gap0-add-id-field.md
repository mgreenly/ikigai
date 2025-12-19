# Task: Add id Field to ik_msg_t

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is the first step in a multi-task refactor to unify `ik_msg_t` and `ik_message_t` into a single canonical message type. The problem:

- `ik_msg_t` (msg.h): `kind`, `content`, `data_json` - used for conversation, serialization
- `ik_message_t` (db/replay.h): `id`, `kind`, `content`, `data_json` - used for DB queries, replay

The only difference is the `id` field. By adding `id` to `ik_msg_t`, we enable later tasks to eliminate `ik_message_t` entirely.

**Sentinel value convention:**
- `id = 0`: Message created in-memory (not yet persisted to DB)
- `id > 0`: Valid DB row ID (PostgreSQL SERIAL starts at 1)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section - lines 15-147)

## Pre-read Source
- src/msg.h (READ - current ik_msg_t definition)

## Source Files to MODIFY
- src/msg.h (MODIFY - add id field to struct)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes

## Task

Add an `int64_t id` field to the `ik_msg_t` struct in `src/msg.h`.

**Change:**
```c
// BEFORE (msg.h lines 29-33)
typedef struct {
    char *kind;
    char *content;
    char *data_json;
} ik_msg_t;

// AFTER
typedef struct {
    int64_t id;       /* DB row ID (0 if not from DB) */
    char *kind;
    char *content;
    char *data_json;
} ik_msg_t;
```

**Requirements:**
1. Add `int64_t id` as the FIRST field in the struct
2. Add comment explaining sentinel value semantics
3. Include `<stdint.h>` if not already included
4. Do NOT modify any other code in this task

## TDD Cycle

### Red
This is a struct modification with no behavioral change. Existing tests should still pass.

1. Run `make check` to confirm all tests pass before modification

### Green
1. Add `#include <stdint.h>` to msg.h (if not present)
2. Add `int64_t id` field with comment to ik_msg_t struct
3. Run `make check` - should pass (uninitialized id is acceptable for now)
4. Run `make lint` - should pass

### Refactor
No refactoring needed for this minimal change.

## Sub-agent Usage
- Use haiku sub-agents to run `make check` and `make lint`

## Post-conditions
- `make check` passes
- `make lint` passes
- `ik_msg_t` struct contains `int64_t id` field as first member
- Working tree is clean (all changes committed)
