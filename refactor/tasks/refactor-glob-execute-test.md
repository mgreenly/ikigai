# Task: Refactor glob_execute_test.c to Use JSON Helpers

## Target

Refactoring: Eliminate JSON parsing duplication in glob_execute_test.c

## Pre-read Skills

- default
- errors
- git
- makefile
- naming
- quality
- scm
- style
- tdd

## Pre-read Source Files

- `tests/test_utils.h` - Test helper functions
- `tests/unit/tool/glob_execute_test.c` - File to refactor
- `src/tool.h` - Tool execution API

## Pre-conditions

- Working tree is clean
- JSON helpers exist and are tested
- Previous tool test files refactored
- All tests pass (`make check`)

## Task

Refactor `tests/unit/tool/glob_execute_test.c` to use JSON helpers.

**Expected test types:**
- Success cases: Glob matches found (may need `ik_test_tool_get_output` for file list)
- Success cases: No matches (empty results)
- Error cases: Invalid pattern

**Note:** Glob results may be in a different format than bash/file_read. Review actual response structure and extract fields accordingly.

## TDD Cycle

### Red: Verify Baseline
Run test file, understand response structure.

### Green: Refactor
Replace parsing with helpers. May need to extract custom fields beyond output/exit_code.

### Refactor: Verify
Run `make check && make lint && make coverage`

## Post-conditions

- File uses JSON helpers where applicable
- All tests pass
- Working tree clean
- Changes committed
