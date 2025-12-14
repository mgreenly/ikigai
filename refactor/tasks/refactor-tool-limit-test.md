# Task: Refactor tool_limit_test.c to Use JSON Helpers

## Target

Refactoring: Eliminate JSON parsing duplication in tool_limit_test.c

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
- `tests/unit/tool/tool_limit_test.c` - File to refactor
- `src/tool.h` - Tool execution API

## Pre-conditions

- Working tree is clean
- JSON helpers exist and are tested
- Previous tool test files refactored
- All tests pass (`make check`)

## Task

Refactor `tests/unit/tool/tool_limit_test.c` to use JSON helpers.

**Expected test types:**
- Tests for tool output limits (truncation, size limits)
- May test multiple tools to verify limit enforcement

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
