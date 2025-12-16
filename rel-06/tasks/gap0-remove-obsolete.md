# Task: Remove Obsolete ik_message_t Code

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 7 of the message type unification - cleanup. Now that all code uses `ik_msg_t`, we can remove:
- The `ik_message_t` typedef (no longer used)
- The `ik_msg_from_db()` function (no longer needed)
- The `ik_msg_from_db_()` wrapper mock (no longer needed)
- The forward declaration in msg.h

**Files to clean up:**
1. src/db/replay.h - remove `ik_message_t` typedef
2. src/msg.h - remove forward declaration and `ik_msg_from_db()` declaration
3. src/msg.c - remove `ik_msg_from_db()` implementation
4. src/wrapper_internal.h - remove `ik_msg_from_db_()` mock
5. src/wrapper.c - remove `ik_msg_from_db_()` implementation

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- src/msg.h (READ - see what to remove)
- src/msg.c (READ - see ik_msg_from_db implementation to remove)
- src/db/replay.h (READ - see ik_message_t typedef to remove)
- src/wrapper_internal.h (READ - see wrapper to remove)
- src/wrapper.c (READ - see wrapper implementation to remove)

## Source Files to MODIFY
- src/db/replay.h (MODIFY - remove ik_message_t typedef)
- src/msg.h (MODIFY - remove forward declaration and ik_msg_from_db declaration)
- src/msg.c (MODIFY - remove ik_msg_from_db function)
- src/wrapper_internal.h (MODIFY - remove ik_msg_from_db_ mock)
- src/wrapper.c (MODIFY - remove ik_msg_from_db_ implementation)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- Code compiles (`make` succeeds)
- Prior tasks completed:
  - gap0-update-session-restore.md (no code uses ik_msg_from_db anymore)

## Task

### Part 1: Remove ik_message_t from db/replay.h

Remove the struct definition and typedef (lines 15-21):
```c
// REMOVE these lines:
struct ik_message {
    int64_t id;        // Message ID from database
    char *kind;        // Event kind (clear, system, user, assistant, mark, rewind)
    char *content;     // Message content
    char *data_json;   // JSONB data as string
};
typedef struct ik_message ik_message_t;
```

### Part 2: Remove from msg.h

1. Remove forward declaration (line 8):
```c
// REMOVE this line:
typedef struct ik_message ik_message_t;
```

2. Remove `ik_msg_from_db()` doc comment and declaration (lines 35-51):
```c
// REMOVE everything from "/**" through "res_t ik_msg_from_db(...);"
```

3. Remove the `#include "db/replay.h"` if it was only needed for ik_message_t

### Part 3: Remove from msg.c

1. Remove `#include "db/replay.h"` (line 7) - no longer needed
2. Remove entire `ik_msg_from_db()` function (lines 13-56)

### Part 4: Remove from wrapper_internal.h

Remove the `ik_msg_from_db_` mock declaration and inline implementation:

In the `#ifdef NDEBUG` block (around line 44-47):
```c
// REMOVE:
MOCKABLE res_t ik_msg_from_db_(void *parent, const ik_message_t *db_msg)
{
    return ik_msg_from_db(parent, db_msg);
}
```

In the `#else` block (around line 68):
```c
// REMOVE:
MOCKABLE res_t ik_msg_from_db_(void *parent, const void *db_msg);
```

### Part 5: Remove from wrapper.c

Remove the `ik_msg_from_db_` implementation (around lines 466-469):
```c
// REMOVE:
MOCKABLE res_t ik_msg_from_db_(void *parent, const void *db_msg)
{
    return ik_msg_from_db(parent, (const ik_message_t *)db_msg);
}
```

## TDD Cycle

### Red
1. Run `make` to confirm current compilation state

### Green
1. Remove ik_message_t from db/replay.h
2. Remove forward declaration and ik_msg_from_db declaration from msg.h
3. Remove ik_msg_from_db function from msg.c
4. Remove ik_msg_from_db_ from wrapper_internal.h (both blocks)
5. Remove ik_msg_from_db_ from wrapper.c
6. Run `make` - should compile
7. Run `make lint` - should pass

### Refactor
No refactoring needed.

## Sub-agent Usage
- Use haiku sub-agents to run `make` and `make lint`
- You may use sub-agents to verify no remaining references with grep

## Post-conditions
- Code compiles (`make` succeeds)
- `make lint` passes
- No `ik_message_t` typedef exists in codebase
- No `ik_msg_from_db` function exists in codebase
- No `ik_msg_from_db_` wrapper exists in codebase
- `grep -r "ik_message_t" src/` returns no results (except possibly comments)
- `grep -r "ik_msg_from_db" src/` returns no results
- Working tree is clean (all changes committed)
