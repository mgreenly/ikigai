# Task: Migrate db/agent_replay to ik_msg_t

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 4 of the message type unification. After migrating `db/replay` to use `ik_msg_t`, we now update `db/agent_replay` which builds on top of it.

**What changes:**
- `ik_agent_query_range()` returns `ik_msg_t***` instead of `ik_message_t***`
- Internal variables use `ik_msg_t*` instead of `ik_message_t*`
- Function signatures in header updated to match

**Why this is straightforward:**
- `ik_msg_t` and `ik_message_t` have identical fields after adding `id`
- This is a mechanical search-and-replace within the file

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- src/msg.h (READ - ik_msg_t definition)
- src/db/replay.h (READ - updated with ik_msg_t)
- src/db/agent_replay.h (READ - function signatures to update)
- src/db/agent_replay.c (READ - implementation to update)

## Source Files to MODIFY
- src/db/agent_replay.h (MODIFY - change function signatures)
- src/db/agent_replay.c (MODIFY - use ik_msg_t throughout)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- Code compiles (`make` succeeds)
- Prior tasks completed:
  - gap0-migrate-replay.md (ik_replay_context_t uses ik_msg_t**)

## Task

### Part 1: Update agent_replay.h

1. Change `ik_agent_query_range()` signature (line 58-61):
```c
// BEFORE
res_t ik_agent_query_range(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                            const ik_replay_range_t *range,
                            ik_message_t ***messages_out,
                            size_t *count_out);

// AFTER
res_t ik_agent_query_range(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                            const ik_replay_range_t *range,
                            ik_msg_t ***messages_out,
                            size_t *count_out);
```

2. Add `#include "../msg.h"` if not already present (for ik_msg_t type)

### Part 2: Update agent_replay.c

Change all `ik_message_t` to `ik_msg_t`. Key locations:

1. **ik_agent_query_range()** (line 180):
   - Parameter: `ik_message_t ***messages_out` -> `ik_msg_t ***messages_out`
   - Local var (line 232): `ik_message_t **messages` -> `ik_msg_t **messages`
   - Allocation (line 236): `ik_message_t *msg` -> `ik_msg_t *msg`

2. **ik_agent_replay_history()** (lines 312, 327, 335, 336):
   - Local vars: `ik_message_t **range_msgs` -> `ik_msg_t **range_msgs`
   - Realloc type: `ik_message_t *` -> `ik_msg_t *`
   - Message copy: `ik_message_t *src_msg` -> `ik_msg_t *src_msg`
   - Message alloc: `ik_message_t *msg` -> `ik_msg_t *msg`

**Search pattern:** Find all `ik_message_t` in agent_replay.c and change to `ik_msg_t` (approximately 10 occurrences).

## TDD Cycle

### Red
1. Run `make` to confirm current compilation state

### Green
1. Update agent_replay.h:
   - Add `#include "../msg.h"` if needed
   - Change `ik_message_t***` to `ik_msg_t***` in ik_agent_query_range

2. Update agent_replay.c:
   - Replace all `ik_message_t` with `ik_msg_t`

3. Run `make` - should compile
4. Run `make lint` - should pass

### Refactor
No refactoring needed at this stage.

## Sub-agent Usage
- Use haiku sub-agents to run `make` and `make lint`
- You may use sub-agents to grep for all `ik_message_t` occurrences and do bulk replacement

## Post-conditions
- Code compiles (`make` succeeds)
- `make lint` passes
- agent_replay.h uses `ik_msg_t***` in function signatures
- agent_replay.c uses `ik_msg_t` throughout
- Working tree is clean (all changes committed)
