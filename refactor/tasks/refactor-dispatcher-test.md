# Task: Refactor dispatcher_test.c to Use JSON Helpers

## Target

Refactoring: Eliminate JSON parsing duplication in dispatcher_test.c

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
- `tests/unit/tool/dispatcher_test.c` - File to refactor
- `src/tool.h` - Tool execution API
- `src/tool_dispatcher.h` - Dispatcher API

## Pre-conditions

- Working tree is clean
- JSON helpers exist and are tested
- All other tool test files refactored (this is the final one)
- All tests pass (`make check`)

## Task

Refactor `tests/unit/tool/dispatcher_test.c` to use JSON helpers.

**Expected test types:**
- Tests for tool dispatcher routing logic
- May dispatch to different tools and verify responses
- Error cases: Unknown tool, invalid arguments

**Note:** This is the final file in the refactoring series.

## TDD Cycle

### Red: Verify Baseline
Run test file.

### Green: Refactor
Replace parsing with helpers.

### Refactor: Verify
Run `make check && make lint && make coverage`

## Post-conditions

- File uses JSON helpers
- All 8 tool test files now refactored
- ~800-1000 lines of boilerplate removed across all files
- All tests pass
- Working tree clean
- Changes committed
- Refactoring series complete!
