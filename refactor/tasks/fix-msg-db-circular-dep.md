# Task: Fix msg.h ↔ db/replay.h Circular Dependency

## Target

Refactor Issue #10: Fix circular dependency between domain layer (`msg.h`) and persistence layer (`db/replay.h`)

## Context

There is a layering violation where:
- `src/msg.h` includes `src/db/replay.h` (line 5)
- The db layer also uses message types from msg

This violates DDD principles:
- Domain entities should not depend on persistence layer
- Persistence should depend on domain, not vice versa

**Current problematic include:**
```c
// src/msg.h:5
#include "db/replay.h"  // Domain depending on persistence - WRONG
```

The dependency exists because `ik_msg_from_db()` converts `ik_message_t` (db type) to OpenAI format. The function signature requires knowledge of the db type.

## Pre-read

### Skills
- default
- database
- errors
- git
- log
- makefile
- naming
- quality
- scm
- source-code
- style
- tdd
- align

### Documentation
- docs/README.md
- .agents/skills/ddd.md (DDD principles)
- .agents/skills/di.md (dependency injection)

### Source Files (Circular Dependency)
- src/msg.h (includes db/replay.h)
- src/msg.c (uses ik_message_t from db)
- src/db/replay.h (defines ik_message_t)
- src/db/message.h (also defines message types)

### Related Source Files
- src/openai/client.h (ik_openai_msg_t - target format)
- src/openai/client_msg.c (message creation functions)

### Test Files
- tests/unit/test_msg.c
- tests/unit/test_replay.c

## Pre-conditions

1. Working tree is clean (`git status --porcelain` returns empty)
2. All tests pass (`make check`)
3. Circular dependency exists (verify with include analysis)

## Task

Break the circular dependency by:
1. Moving shared types to a separate header (if needed)
2. Inverting the dependency direction (persistence depends on domain)
3. Or using forward declarations to break the cycle

## Analysis

### Option A: Forward Declaration
If `msg.h` only needs `ik_message_t*` (pointer), use forward declaration:
```c
// msg.h
struct ik_message;  // Forward declaration
typedef struct ik_message ik_message_t;

res_t ik_msg_from_db(void *parent, const ik_message_t *db_msg, ...);
```

### Option B: Shared Types Header
Create `src/message_types.h` with shared type definitions:
```c
// message_types.h - shared by both layers
typedef struct ik_message {
    // fields
} ik_message_t;
```

Then both `msg.h` and `db/replay.h` include `message_types.h`.

### Option C: Move Function to DB Layer
Move `ik_msg_from_db()` to `db/` since it's a persistence-to-domain conversion:
```c
// db/message.h
res_t ik_db_msg_to_openai(...);
```

**Recommended: Option A or B** - Forward declaration is simplest if only pointers are used.

## TDD Cycle

### Red Phase

1. Verify the circular dependency exists:
   ```bash
   grep -n '#include.*db/replay.h' src/msg.h
   grep -n '#include.*msg.h' src/db/*.h
   ```

2. Create a test that verifies the fix (compilation test):
   - Add test in `test_msg.c` that uses the refactored API
   - Test should compile and pass after fix

### Green Phase

1. **If using forward declaration:**
   - Remove `#include "db/replay.h"` from `msg.h`
   - Add forward declaration: `struct ik_message; typedef struct ik_message ik_message_t;`
   - Add `#include "db/replay.h"` in `msg.c` (implementation needs full type)
   - Verify compilation: `make clean && make`

2. **If using shared types header:**
   - Create `src/message_types.h` with `ik_message_t` definition
   - Update `msg.h` to include `message_types.h` instead of `db/replay.h`
   - Update `db/replay.h` to include `message_types.h`
   - Verify compilation: `make clean && make`

3. Run `make check` - all tests should pass.

### Refactor Phase

1. Verify no other circular dependencies introduced:
   ```bash
   # Check that db layer doesn't include msg.h
   grep -r '#include.*"msg.h"' src/db/
   ```

2. Update any other files affected by the include change.

3. Run `make lint` - verify no new warnings.

4. Run `make coverage` - verify coverage maintained.

## Post-conditions

1. `src/msg.h` no longer includes `src/db/replay.h` directly
2. Dependency direction is correct: persistence → domain (not domain → persistence)
3. All tests pass (`make check`)
4. Lint passes (`make lint`)
5. Coverage maintained at 100% (`make coverage`)
6. Working tree is clean (changes committed)

## Commit Message Format

### If using forward declaration:
```
refactor: break msg.h → db/replay.h circular dependency

- Use forward declaration of ik_message_t in msg.h
- Move full include to msg.c implementation
- Fixes DDD layering: domain no longer depends on persistence
```

### If using shared types header:
```
refactor: break msg.h → db/replay.h circular dependency

- Create message_types.h with shared ik_message_t definition
- Both msg.h and db/replay.h now include message_types.h
- Fixes DDD layering violation
```

## Risk Assessment

**Risk: Medium**
- Include changes can have cascading effects
- Need to verify all compilation units still build
- Existing tests verify behavior preserved

## Estimated Complexity

**Medium** - Requires careful analysis of include dependencies and possible API changes
