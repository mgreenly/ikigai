# Task: Delete Obsolete msg_from_db Tests

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 12 of the message type unification. The `msg_from_db_test.c` file tests the `ik_msg_from_db()` function which no longer exists. The entire test file should be deleted.

**Files to delete:**
- tests/unit/msg/msg_from_db_test.c

**Build system update:**
- The test is likely registered in a Makefile or CMakeLists.txt that needs updating

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- tests/unit/msg/msg_from_db_test.c (READ - confirm it only tests ik_msg_from_db)
- Makefile or tests/Makefile (READ - find test registration)

## Source Files to DELETE
- tests/unit/msg/msg_from_db_test.c (DELETE)

## Source Files to MODIFY
- Likely: Makefile or tests build configuration (MODIFY - remove test reference)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make` succeeds
- Prior tasks completed:
  - gap0-remove-obsolete.md (ik_msg_from_db no longer exists)

## Task

### Part 1: Delete the test file

```bash
rm tests/unit/msg/msg_from_db_test.c
```

### Part 2: Update build system

Search for references to `msg_from_db_test` in the build configuration:

```bash
grep -r "msg_from_db_test" Makefile* CMakeLists* tests/
```

Remove any references found. Common locations:
- Makefile (TEST_SOURCES or similar variable)
- tests/Makefile
- CMakeLists.txt

### Part 3: Verify tests directory structure

After deletion, check if `tests/unit/msg/` directory is empty. If so, consider removing the empty directory:

```bash
rmdir tests/unit/msg/  # Only if empty
```

## TDD Cycle

### Red
1. Run `make check` - should pass before deletion (test compiles but function doesn't exist, so may already fail)

### Green
1. Delete tests/unit/msg/msg_from_db_test.c
2. Update build configuration to remove references
3. Remove empty directory if applicable
4. Run `make check` - should pass
5. Run `make lint` - should pass

### Refactor
No refactoring needed.

## Sub-agent Usage
- Use haiku sub-agents to run `make check` and `make lint`
- Use haiku sub-agents to grep for build system references

## Post-conditions
- `make check` passes
- `make lint` passes
- tests/unit/msg/msg_from_db_test.c does not exist
- No references to msg_from_db_test in build system
- Working tree is clean (all changes committed)
