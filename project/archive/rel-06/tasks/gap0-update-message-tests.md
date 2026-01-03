# Task: Update Message Tool Result Tests

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 10 of the message type unification. The `message_tool_result_test.c` uses `ik_message_t*` to receive the return value from `ik_msg_create_tool_result()`. Since that function now returns `ik_msg_t*`, we update the tests.

**Test to update:**
- tests/unit/db/message_tool_result_test.c

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- src/msg.h (READ - ik_msg_t definition)
- src/db/message.h (READ - updated function signature)
- tests/unit/db/message_tool_result_test.c (READ - see ik_message_t usage)

## Source Files to MODIFY
- tests/unit/db/message_tool_result_test.c (MODIFY - use ik_msg_t)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- Code compiles (`make` succeeds)
- Prior tasks completed:
  - gap0-migrate-message.md (ik_msg_create_tool_result returns ik_msg_t*)

## Task

### Update message_tool_result_test.c

1. Add `#include "../../../src/msg.h"` if not present

2. Change all `ik_message_t *msg` declarations to `ik_msg_t *msg`:

```c
// BEFORE (appears at lines 27, 39, 53, 67, 82, 97, 112, 134, 160, 175)
ik_message_t *msg = ik_msg_create_tool_result(...);

// AFTER
ik_msg_t *msg = ik_msg_create_tool_result(...);
```

**Search and replace:** `ik_message_t *msg` -> `ik_msg_t *msg` (approximately 10 occurrences)

## TDD Cycle

### Red
1. Run `make check` - tests should fail to compile due to type mismatch

### Green
1. Add `#include "../../../src/msg.h"` if not present
2. Replace all `ik_message_t *msg` with `ik_msg_t *msg`
3. Run `make check` - tests should compile and pass
4. Run `make lint` - should pass

### Refactor
No refactoring needed.

## Sub-agent Usage
- Use haiku sub-agents to run `make check` and `make lint`

## Post-conditions
- `make check` passes
- `make lint` passes
- No `ik_message_t` references in message_tool_result_test.c
- Working tree is clean (all changes committed)
