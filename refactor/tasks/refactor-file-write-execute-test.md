# Task: Refactor file_write_execute_test.c to Use JSON Helpers

## Target

Refactoring: Eliminate JSON parsing duplication in file_write_execute_test.c

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
- `tests/unit/tool/file_write_execute_test.c` - File to refactor
- `src/tool.h` - Tool execution API

## Pre-conditions

- Working tree is clean
- JSON helpers exist and are tested
- Previous tool test files refactored (provides pattern)
- All tests pass (`make check`)

## Task

Refactor `tests/unit/tool/file_write_execute_test.c` to use JSON helpers.

**Expected test types:**
- Success cases: File written successfully
- Error cases: Permission denied, invalid path, disk full, etc.

**Pattern:**
- Success cases: `ik_test_tool_parse_success` + `ik_test_tool_get_output`
- Error cases: `ik_test_tool_parse_error`

## TDD Cycle

### Red: Verify Baseline
Run the test file to establish baseline.

### Green: Refactor
Replace JSON parsing boilerplate with helpers. Commit incrementally.

### Refactor: Verify
Run `make check && make lint && make coverage`

## Post-conditions

- File uses JSON helpers
- All tests pass
- Working tree clean
- Changes committed
