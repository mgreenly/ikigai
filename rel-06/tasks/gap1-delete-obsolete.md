# Task: Delete Obsolete Session Restore Files

## Target
Gap 1 + Gap 5 merged: Startup Agent Restoration Loop (PR2) - Final Cleanup

## Macro Context

This task removes legacy code that is no longer needed after the agent-based restoration system is complete.

**What is being removed:**

1. **Legacy session_restore module** - The original session restoration system that was replaced by the new agent-based restoration in `src/repl/agent_restore.c`. This module used session-based replay (`ik_db_messages_load()`) which doesn't understand:
   - Agent-scoped messages
   - Ancestor chain walking
   - Fork point boundaries

2. **`ik_db_messages_load()` function** - The session-based message loading function that has been superseded by `ik_agent_replay_history()`. This function only takes `session_id`, not `agent_uuid`, and cannot handle the multi-agent data model.

**Why it's safe to remove:**

1. The `gap1-fresh-install.md` task (predecessor) migrated all functionality to the agent-based system
2. Agent 0 restoration now uses `ik_agent_replay_history()` instead of `ik_db_messages_load()`
3. All other agents use the same agent-based restoration path
4. Fresh install handling has been moved to `ik_repl_restore_agents()`
5. The test files for session_restore were deleted in a prior task when the functionality was migrated

**Files being removed:**

| File | Reason |
|------|--------|
| `src/repl/session_restore.c` | Entire legacy module - replaced by agent_restore.c |
| `src/repl/session_restore.h` | Header for deleted module |
| `src/db/replay.c:ik_db_messages_load()` | Session-based loading - replaced by agent replay |
| `src/db/replay.h:ik_db_messages_load()` | Declaration for deleted function |

**Note on `ik_db_messages_load()`:** Before deleting this function, the agent MUST verify no remaining callers exist. If callers remain, document them and leave the function in place.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/source-code.md
- .agents/skills/style.md
- .agents/skills/naming.md

## Pre-read Docs
- rel-06/gap.md (Gap 1 AND Gap 5 sections, especially "Code Removal Summary")

## Pre-read Source

**Files to DELETE (read first to understand what's being removed):**
- src/repl/session_restore.c (READ - entire file will be deleted)
- src/repl/session_restore.h (READ - entire file will be deleted)

**Files to MODIFY (read to understand current state):**
- src/db/replay.c (READ - contains `ik_db_messages_load()` to potentially remove)
- src/db/replay.h (READ - contains `ik_db_messages_load()` declaration)
- Makefile (READ - contains explicit references to session_restore.c)

**Files that should NOT reference session_restore anymore:**
- src/repl_init.c (READ - verify no include of session_restore.h)
- src/wrapper.c (READ - may have mock wrapper for ik_repl_restore_session_)
- src/wrapper_internal.h (READ - may have wrapper declaration)

## Source Files to DELETE
- src/repl/session_restore.c (DELETE entire file)
- src/repl/session_restore.h (DELETE entire file)

## Source Files to MODIFY
- src/db/replay.c (MODIFY - remove `ik_db_messages_load()` if no callers)
- src/db/replay.h (MODIFY - remove `ik_db_messages_load()` declaration if function deleted)
- Makefile (MODIFY - remove session_restore.c from source lists)
- src/wrapper.c (MODIFY - remove `ik_repl_restore_session_` wrapper if present)
- src/wrapper_internal.h (MODIFY - remove wrapper declaration if present)

## Test Files to DELETE
- tests/unit/repl/session_restore_test.c (DELETE if exists)
- tests/unit/repl/session_restore_marks_test.c (DELETE if exists)
- tests/unit/repl/session_restore_errors_test.c (DELETE if exists)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Prior task completed: gap1-fresh-install.md
- Agent-based restoration is fully functional (tested by prior tasks)

## Task

### Part 1: Verify no remaining callers (CRITICAL)

Before deleting ANY code, verify there are no remaining references:

```bash
# Search for any remaining references to ik_repl_restore_session
grep -r "ik_repl_restore_session" src/

# Search for any remaining references to ik_db_messages_load
grep -r "ik_db_messages_load" src/

# Search for session_restore.h includes
grep -r "session_restore.h" src/
```

**Expected results:**
- `ik_repl_restore_session` should only appear in:
  - `src/repl/session_restore.c` (the function definition)
  - `src/repl/session_restore.h` (the declaration)
  - `src/wrapper.c` and `src/wrapper_internal.h` (wrapper - also to be removed)

- `ik_db_messages_load` should only appear in:
  - `src/db/replay.c` (the function definition)
  - `src/db/replay.h` (the declaration)
  - `src/repl/session_restore.c` (the only caller - being deleted)

**If unexpected callers are found:**
1. Document the callers in your final report
2. Do NOT delete the code
3. Create a follow-up task to handle the remaining callers

### Part 2: Delete session_restore files

If Part 1 verification passes:

```bash
# Delete the source files
rm src/repl/session_restore.c
rm src/repl/session_restore.h
```

### Part 3: Delete associated test files

```bash
# Delete test files (if they exist)
rm -f tests/unit/repl/session_restore_test.c
rm -f tests/unit/repl/session_restore_marks_test.c
rm -f tests/unit/repl/session_restore_errors_test.c
```

### Part 4: Remove ik_db_messages_load (if orphaned)

If verification in Part 1 confirms no callers remain after session_restore.c deletion:

1. **In src/db/replay.c:** Remove the entire `ik_db_messages_load()` function (approximately lines 270-354)

2. **In src/db/replay.h:** Remove the function declaration and associated documentation comment (approximately lines 59-80)

### Part 5: Update Makefile

Remove `src/repl/session_restore.c` from the following variables in Makefile:

1. `CLIENT_SOURCES` (around line 99)
2. `MODULE_SOURCES` (around line 114)
3. `MODULE_SOURCES_NO_DB` (around line 118)

Also remove the special test rules for session_restore tests:
- Rule for `session_restore_test` (around line 237)
- Rule for `session_restore_marks_test` (around line 242)
- Rule for `session_restore_errors_test` (around line 247)

### Part 6: Remove wrapper functions (if present)

Check `src/wrapper.c` and `src/wrapper_internal.h` for:
- `ik_repl_restore_session_()` wrapper function
- Associated declaration

If found, remove them as the underlying function no longer exists.

### Part 7: Verify repl_init.c is clean

Verify that `src/repl_init.c`:
1. Does NOT include `session_restore.h`
2. Does NOT call `ik_repl_restore_session()` or `ik_repl_restore_session_()`
3. Now uses `ik_repl_restore_agents()` for session restoration

If the include or call remains, this task cannot proceed - report the issue.

### Part 8: Final verification

```bash
# Verify no remaining references
grep -r "session_restore" src/
grep -r "ik_db_messages_load" src/

# Build should succeed
make clean && make

# All tests should pass
make check

# Lint should pass
make lint
```

## TDD Cycle

### Red
1. Run `make` to verify current state builds
2. Run `make check` to verify all tests pass before changes

### Green
1. Perform verification searches (Part 1)
2. Delete session_restore files (Part 2)
3. Delete test files (Part 3)
4. Remove `ik_db_messages_load()` if orphaned (Part 4)
5. Update Makefile (Part 5)
6. Remove wrapper functions (Part 6)
7. Verify repl_init.c is clean (Part 7)
8. Run `make` - should compile successfully
9. Run `make check` - all tests should pass

### Refactor
1. Verify no dead code remains
2. Run `make lint` to ensure code quality
3. Run final grep searches to confirm complete removal

## Sub-agent Usage

Use sub-agents for:
- Running grep searches to find callers
- Running `make` and `make check` for quick feedback
- Verifying no remaining references after deletion

## Post-conditions
- `src/repl/session_restore.c` does not exist
- `src/repl/session_restore.h` does not exist
- `ik_db_messages_load()` is removed from replay.c (or documented why it remains)
- Makefile no longer references session_restore.c
- No `#include "session_restore.h"` anywhere in codebase
- No `ik_repl_restore_session` references anywhere in codebase
- `make` compiles successfully
- `make check` passes (all tests)
- `make lint` passes
- Working tree is clean (all changes committed)

## Deviations

If you need to deviate from this plan (e.g., callers remain for `ik_db_messages_load`, additional files need modification, wrapper removal causes issues), document the deviation and reasoning in your final report.

**Common deviations to watch for:**

1. **Remaining callers for `ik_db_messages_load()`**: If callers exist outside session_restore.c, DO NOT delete the function. Document the callers and leave the function in place.

2. **Test files already deleted**: Prior tasks may have already removed the test files. This is expected behavior - simply note it in your report.

3. **repl_init.c still has include**: If the include hasn't been removed by prior tasks, this is a blocker. Report the issue rather than modifying repl_init.c (that was a prior task's responsibility).

4. **Build failures after deletion**: If the build fails, there are remaining references not caught by grep. Investigate and document before proceeding.
