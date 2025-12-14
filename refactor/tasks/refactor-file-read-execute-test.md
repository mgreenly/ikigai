# Task: Refactor file_read_execute_test.c to Use JSON Helpers

## Target

Refactoring: Eliminate JSON parsing duplication in file_read_execute_test.c

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
- `tests/unit/tool/file_read_execute_test.c` - File to refactor
- `src/tool.h` - Tool execution API

## Pre-conditions

- Working tree is clean
- JSON helpers exist and are tested
- `bash_execute_test.c` has been refactored (provides pattern example)
- All tests pass (`make check`)

## Task

Refactor `tests/unit/tool/file_read_execute_test.c` to use JSON helpers, eliminating duplicated parsing code.

**Tests to refactor:**
- `test_file_read_exec_valid_file` - Success with content
- `test_file_read_exec_file_not_found` - Error case (use `parse_error`)
- `test_file_read_exec_permission_denied` - Error case (use `parse_error`)
- `test_file_read_exec_empty_file` - Success with empty output
- Any other tests in the file

**Pattern:**
- Success cases: Use `ik_test_tool_parse_success` + `ik_test_tool_get_output`
- Error cases: Use `ik_test_tool_parse_error`

## TDD Cycle

### Red: Verify Baseline
Run `make build/tests/unit/tool/file_read_execute_test && ./build/tests/unit/tool/file_read_execute_test`

### Green: Refactor
Refactor each test using helpers. Commit after each test or small group.

### Refactor: Verify
Run `make check && make lint && make coverage`

## Post-conditions

- File uses JSON helpers throughout
- All tests pass
- Working tree clean
- Changes committed
