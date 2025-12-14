# Task: Refactor grep_edge_cases_test.c to Use JSON Helpers

## Target

Refactoring: Eliminate JSON parsing duplication in grep_edge_cases_test.c

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
- `tests/unit/tool/grep_edge_cases_test.c` - File to refactor
- `src/tool.h` - Tool execution API

## Pre-conditions

- Working tree is clean
- JSON helpers exist and are tested
- `grep_execute_test.c` already refactored (same tool, similar patterns)
- All tests pass (`make check`)

## Task

Refactor `tests/unit/tool/grep_edge_cases_test.c` to use JSON helpers.

**Expected test types:**
- Edge cases for grep: empty files, special characters, binary files, etc.
- Pattern should match `grep_execute_test.c` refactoring

## TDD Cycle

### Red: Verify Baseline
Run test file.

### Green: Refactor
Replace parsing with helpers.

### Refactor: Verify
Run `make check && make lint && make coverage`

## Post-conditions

- File uses JSON helpers
- All tests pass
- Working tree clean
- Changes committed
