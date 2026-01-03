# Task: Update Replay Tests

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 8 of the message type unification. The replay tests use `ik_message_t` which no longer exists. We update them to use `ik_msg_t`.

**Tests to update:**
- tests/unit/db/agent_replay_test.c
- tests/unit/db/agent_replay_errors_test.c

These tests declare variables like `ik_message_t **messages` which must become `ik_msg_t **messages`.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- src/msg.h (READ - ik_msg_t definition)
- tests/unit/db/agent_replay_test.c (READ - see ik_message_t usage)
- tests/unit/db/agent_replay_errors_test.c (READ - see ik_message_t usage)

## Source Files to MODIFY
- tests/unit/db/agent_replay_test.c (MODIFY - use ik_msg_t)
- tests/unit/db/agent_replay_errors_test.c (MODIFY - use ik_msg_t)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- Code compiles (`make` succeeds)
- Prior tasks completed:
  - gap0-remove-obsolete.md (ik_message_t no longer exists)

## Task

### Part 1: Update agent_replay_test.c

1. Add `#include "../../../src/msg.h"` if not present

2. Change all `ik_message_t` to `ik_msg_t`. Key locations:
   - Line 384: `ik_message_t **messages = NULL;`
   - Line 409: `ik_message_t **messages = NULL;`
   - Line 438: `ik_message_t **all_msgs = NULL;`
   - Line 452: `ik_message_t **messages = NULL;`

**Search and replace:** `ik_message_t` -> `ik_msg_t` throughout the file

### Part 2: Update agent_replay_errors_test.c

1. Add `#include "../../../src/msg.h"` if not present

2. Change all `ik_message_t` to `ik_msg_t`. Key locations:
   - Line 193: `ik_message_t **messages = NULL;`
   - Line 222: `ik_message_t **messages = NULL;`
   - Line 253: `ik_message_t **messages = NULL;`
   - Line 286: `ik_message_t **messages = NULL;`

**Search and replace:** `ik_message_t` -> `ik_msg_t` throughout the file

## TDD Cycle

### Red
1. Run `make check` - tests should fail to compile due to unknown type

### Green
1. Update agent_replay_test.c:
   - Add include if needed
   - Replace all ik_message_t with ik_msg_t

2. Update agent_replay_errors_test.c:
   - Add include if needed
   - Replace all ik_message_t with ik_msg_t

3. Run `make check` - tests should compile and pass
4. Run `make lint` - should pass

### Refactor
No refactoring needed.

## Sub-agent Usage
- Use haiku sub-agents to run `make check` and `make lint`
- You may use sub-agents to do bulk search-and-replace operations

## Post-conditions
- `make check` passes
- `make lint` passes
- No `ik_message_t` references in agent_replay_test.c
- No `ik_message_t` references in agent_replay_errors_test.c
- Working tree is clean (all changes committed)
