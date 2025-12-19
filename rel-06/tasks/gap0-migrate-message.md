# Task: Migrate db/message to ik_msg_t

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 5 of the message type unification. The function `ik_msg_create_tool_result()` currently returns `ik_message_t*`. We change it to return `ik_msg_t*`.

**What changes:**
- `ik_msg_create_tool_result()` return type: `ik_message_t*` -> `ik_msg_t*`
- Internal implementation uses `ik_msg_t` instead of `ik_message_t`

**Impact:**
- Tests that use `ik_msg_create_tool_result()` will need updates (later task)
- Integration tests that use this function will need updates (later task)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- src/msg.h (READ - ik_msg_t definition)
- src/db/message.h (READ - ik_msg_create_tool_result declaration)
- src/db/message.c (READ - ik_msg_create_tool_result implementation)

## Source Files to MODIFY
- src/db/message.h (MODIFY - change return type)
- src/db/message.c (MODIFY - use ik_msg_t)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- Code compiles (`make` succeeds)
- Prior tasks completed:
  - gap0-migrate-replay.md
  - gap0-migrate-agent-replay.md

## Task

### Part 1: Update message.h

1. Add `#include "../msg.h"` to get ik_msg_t definition (if not already present)

2. Change `ik_msg_create_tool_result()` return type (line 70):
```c
// BEFORE
ik_message_t *ik_msg_create_tool_result(void *parent,
                                         const char *tool_call_id,
                                         const char *name,
                                         const char *output,
                                         bool success,
                                         const char *content);

// AFTER
ik_msg_t *ik_msg_create_tool_result(void *parent,
                                     const char *tool_call_id,
                                     const char *name,
                                     const char *output,
                                     bool success,
                                     const char *content);
```

3. Update the doc comment (line 53) to reference `ik_msg_t` instead of `ik_message_t`:
```c
// BEFORE
 * Allocates an ik_message_t struct representing a tool execution result

// AFTER
 * Allocates an ik_msg_t struct representing a tool execution result
```

4. Update line 68:
```c
// BEFORE
 * @return               Allocated ik_message_t struct (owned by parent), or NULL on OOM

// AFTER
 * @return               Allocated ik_msg_t struct (owned by parent), or NULL on OOM
```

### Part 2: Update message.c

1. Change function signature and local variable (lines 99, 111):
```c
// BEFORE (line 99)
ik_message_t *ik_msg_create_tool_result(void *parent, ...)

// AFTER
ik_msg_t *ik_msg_create_tool_result(void *parent, ...)

// BEFORE (line 111)
ik_message_t *msg = talloc_zero(parent, ik_message_t);

// AFTER
ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
```

2. Initialize `msg->id = 0` after allocation to indicate in-memory message.

## TDD Cycle

### Red
1. Run `make` to confirm current compilation state

### Green
1. Update message.h:
   - Add `#include "../msg.h"` if needed
   - Change return type and doc comments

2. Update message.c:
   - Change function signature
   - Change local variable type
   - Add `msg->id = 0;` after allocation

3. Run `make` - may have warnings in tests (expected)
4. Run `make lint` - should pass

### Refactor
No refactoring needed.

## Sub-agent Usage
- Use haiku sub-agents to run `make` and `make lint`

## Post-conditions
- Code compiles (`make` succeeds)
- `make lint` passes
- `ik_msg_create_tool_result()` returns `ik_msg_t*`
- Working tree is clean (all changes committed)

## Note on Test/Integration Failures

Tests using `ik_msg_create_tool_result()` may fail because they expect `ik_message_t*`. These are fixed in later tasks:
- gap0-update-message-tests.md
- gap0-update-integration-tests.md
