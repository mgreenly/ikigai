# Task: Update Session Restore Tests

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 9 of the message type unification. The session_restore tests have:
1. Mocks for `ik_msg_from_db_()` which no longer exists
2. Helper functions creating `ik_message_t` objects which no longer exist
3. Mock replay contexts using `ik_message_t**`

All of these need to be updated to use `ik_msg_t`.

**Tests to update:**
- tests/unit/repl/session_restore_test.c
- tests/unit/repl/session_restore_errors_test.c
- tests/unit/repl/session_restore_marks_test.c

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- src/msg.h (READ - ik_msg_t definition)
- tests/unit/repl/session_restore_test.c (READ - see mocks and helpers)
- tests/unit/repl/session_restore_errors_test.c (READ - see mocks and helpers)
- tests/unit/repl/session_restore_marks_test.c (READ - see mocks and helpers)

## Source Files to MODIFY
- tests/unit/repl/session_restore_test.c (MODIFY)
- tests/unit/repl/session_restore_errors_test.c (MODIFY)
- tests/unit/repl/session_restore_marks_test.c (MODIFY)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- Code compiles (`make` succeeds)
- Prior tasks completed:
  - gap0-remove-obsolete.md (ik_message_t and ik_msg_from_db no longer exist)

## Task

### Changes for ALL THREE test files:

#### 1. Remove ik_msg_from_db_ mock

Each file has a mock implementation like:
```c
// REMOVE this function entirely:
MOCKABLE res_t ik_msg_from_db_(void *parent, const void *db_msg) {
    return ik_msg_from_db(parent, (const ik_message_t *)db_msg);
}
```

#### 2. Update create_mock_message helper

Each file has a helper like:
```c
// BEFORE
static ik_message_t *create_mock_message(TALLOC_CTX *ctx, const char *kind, const char *content)
{
    ik_message_t *msg = talloc_zero_(ctx, sizeof(ik_message_t));
    ...
}

// AFTER
static ik_msg_t *create_mock_message(TALLOC_CTX *ctx, const char *kind, const char *content)
{
    ik_msg_t *msg = talloc_zero_(ctx, sizeof(ik_msg_t));
    msg->id = 0;  // In-memory message
    ...
}
```

#### 3. Update mock replay context allocations

Each file allocates mock replay contexts:
```c
// BEFORE
replay_ctx->messages = talloc_array_(ctx, sizeof(ik_message_t *), count);

// AFTER
replay_ctx->messages = talloc_array_(ctx, sizeof(ik_msg_t *), count);
```

#### 4. Add include for ik_msg_t

Ensure `#include "../../../src/msg.h"` is present.

### File-specific changes:

#### session_restore_test.c
- Line 89-90: Remove ik_msg_from_db_ mock
- Line 140: Update array allocation
- Lines 144-146: Update create_mock_message

#### session_restore_errors_test.c
- Lines 42, 191-192: Remove mock state and ik_msg_from_db_ mock
- Lines 280-282: Update create_mock_message
- Lines 384, 462, 486: Update array allocations
- Line 452: Remove test for ik_msg_from_db failure (function no longer exists)

#### session_restore_marks_test.c
- Lines 101-102: Remove ik_msg_from_db_ mock
- Lines 156-158: Update create_mock_message
- Lines 179, 236, 273, 311: Update array allocations

## TDD Cycle

### Red
1. Run `make check` - tests should fail to compile

### Green
1. For each of the three test files:
   - Remove ik_msg_from_db_ mock function
   - Update create_mock_message to use ik_msg_t
   - Update talloc_array_ calls to use sizeof(ik_msg_t *)
   - Add msg.h include if needed

2. In session_restore_errors_test.c specifically:
   - Remove test_ik_msg_from_db_fails test case if it exists
   - Remove from suite registration

3. Run `make check` - tests should compile and pass
4. Run `make lint` - should pass

### Refactor
No refactoring needed.

## Sub-agent Usage
- Use haiku sub-agents to run `make check` and `make lint`
- You may use sub-agents to handle each test file in parallel

## Post-conditions
- `make check` passes
- `make lint` passes
- No `ik_message_t` references in any session_restore test file
- No `ik_msg_from_db_` references in any session_restore test file
- Working tree is clean (all changes committed)
