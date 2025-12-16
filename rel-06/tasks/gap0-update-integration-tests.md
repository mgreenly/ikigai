# Task: Update Integration Tests

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 11 of the message type unification. Several integration tests use `ik_message_t*` to receive the return value from `ik_msg_create_tool_result()`. Since that function now returns `ik_msg_t*`, we update the tests.

**Tests to update:**
- tests/integration/tool_choice_required_test.c
- tests/integration/tool_choice_specific_test.c
- tests/integration/bash_command_error_test.c
- tests/integration/tool_loop_limit_test.c
- tests/integration/file_read_error_test.c
- tests/integration/tool_choice_auto_test.c

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- src/msg.h (READ - ik_msg_t definition)
- src/db/message.h (READ - updated function signature)

## Source Files to MODIFY
- tests/integration/tool_choice_required_test.c (MODIFY)
- tests/integration/tool_choice_specific_test.c (MODIFY)
- tests/integration/bash_command_error_test.c (MODIFY)
- tests/integration/tool_loop_limit_test.c (MODIFY)
- tests/integration/file_read_error_test.c (MODIFY)
- tests/integration/tool_choice_auto_test.c (MODIFY)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- Code compiles (`make` succeeds)
- Prior tasks completed:
  - gap0-migrate-message.md (ik_msg_create_tool_result returns ik_msg_t*)

## Task

### For each integration test file:

1. Add `#include "../../src/msg.h"` if not present

2. Change `ik_message_t *tool_result_msg` to `ik_msg_t *tool_result_msg`

**Specific locations:**

#### tool_choice_required_test.c
- Line 265: `ik_message_t *tool_result_msg`

#### tool_choice_specific_test.c
- Line 282: `ik_message_t *tool_result_msg`

#### bash_command_error_test.c
- Line 325: `ik_message_t *tool_result_msg`
- Line 425: `ik_message_t *tool_result_msg`

#### tool_loop_limit_test.c
- Line 199: `ik_message_t *tool_result_msg_1`
- Line 226: `ik_message_t *tool_result_msg_2`
- Line 262: `ik_message_t *tool_result_msg_3`

#### file_read_error_test.c
- Line 189: `ik_message_t *tool_result_msg`

#### tool_choice_auto_test.c
- Line 265: `ik_message_t *tool_result_msg`

**Search pattern:** In each file, find `ik_message_t *tool_result_msg` and change to `ik_msg_t *tool_result_msg`

## TDD Cycle

### Red
1. Run `make check` - integration tests should fail to compile

### Green
1. For each of the six integration test files:
   - Add `#include "../../src/msg.h"` if needed
   - Replace `ik_message_t *` with `ik_msg_t *` for tool result variables

2. Run `make check` - tests should compile and pass
3. Run `make lint` - should pass

### Refactor
No refactoring needed.

## Sub-agent Usage
- Use haiku sub-agents to run `make check` and `make lint`
- You may use sub-agents to handle multiple test files in parallel for faster completion

## Post-conditions
- `make check` passes
- `make lint` passes
- No `ik_message_t` references in any of the integration test files
- Working tree is clean (all changes committed)
