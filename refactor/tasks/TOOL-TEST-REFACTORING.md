# Tool Test JSON Parsing Refactoring - Task Series

## Overview

This task series eliminates ~800-1000 lines of duplicated JSON parsing boilerplate from tool execution tests.

## Problem Statement

Tool execution tests contain repetitive 20-30 line JSON parsing sequences in every test:
```c
yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
ck_assert_ptr_nonnull(doc);
yyjson_val *root = yyjson_doc_get_root(doc);
// ... 15-20 more lines ...
yyjson_doc_free(doc);
```

This pattern appears in **~50+ tests across 8 files**, making tests verbose and maintenance-heavy.

## Solution

Create reusable test helper functions that encapsulate JSON parsing and validation:
- `ik_test_tool_parse_success()` - Parse and verify success response
- `ik_test_tool_parse_error()` - Parse and verify error response
- `ik_test_tool_get_output()` - Extract output field
- `ik_test_tool_get_exit_code()` - Extract exit_code field

## Task Breakdown

### 1. Foundation Task (Requires Extended Thinking)
**`tool-test-helpers.md`** - Create helper functions and tests
- Model: sonnet, Thinking: extended
- Creates 4 new helper functions in `tests/test_utils.{h,c}`
- Adds comprehensive test coverage in `tests/unit/test_utils/tool_json_helpers_test.c`
- Foundation for all subsequent tasks

### 2-9. Refactoring Tasks (Standard Thinking)
Each task refactors one test file using the new helpers:

| Task | File | Tests Affected |
|------|------|----------------|
| `refactor-bash-execute-test.md` | `bash_execute_test.c` | ~6-8 tests |
| `refactor-file-read-execute-test.md` | `file_read_execute_test.c` | ~4-6 tests |
| `refactor-file-write-execute-test.md` | `file_write_execute_test.c` | ~4-6 tests |
| `refactor-glob-execute-test.md` | `glob_execute_test.c` | ~4-6 tests |
| `refactor-grep-execute-test.md` | `grep_execute_test.c` | ~6-8 tests |
| `refactor-grep-edge-cases-test.md` | `grep_edge_cases_test.c` | ~4-6 tests |
| `refactor-tool-limit-test.md` | `tool_limit_test.c` | ~2-4 tests |
| `refactor-dispatcher-test.md` | `dispatcher_test.c` | ~4-6 tests |

All refactoring tasks use: Model: sonnet, Thinking: thinking

## Execution Order

Tasks **must** be executed in order:
1. `tool-test-helpers.md` first (creates the helpers)
2. Then tasks 2-9 in sequence (each builds confidence in the pattern)

The `order.json` file enforces this sequence.

## Expected Impact

### Quantitative
- **~800-1000 lines removed** from test files
- **~150-200 lines added** for helper implementation and tests
- **Net reduction: ~650-800 lines** of code
- **8 test files** made more maintainable

### Qualitative
- Tests focus on **behavior** instead of JSON mechanics
- Centralized JSON parsing logic (one place to fix bugs)
- Consistent test style across all tool tests
- Easier to add new tool tests (follow simple pattern)

## Pre-reads

All tasks include comprehensive pre-reads:
- **Skills**: default, database, errors, git, log, makefile, naming, quality, scm, source-code, style, tdd, align
- **Source files**: Specific files relevant to each task
- **Documentation**: Relevant project docs

## Success Criteria

After all tasks complete:
- All tests pass (`make check`)
- Lint passes (`make lint`)
- Coverage maintained at 100% (`make coverage`)
- JSON parsing boilerplate eliminated
- Code is more maintainable and readable

## How to Run

Use the orchestrator to execute the task series:
```bash
/orchestrate refactor/tasks
```

The orchestrator will:
1. Execute tasks in order from `order.json`
2. Handle retries with escalation if needed
3. Track progress and timing
4. Report completion status

Each sub-agent will:
- Verify clean working tree before starting
- Follow TDD cycle (Red-Green-Refactor)
- Commit incrementally
- Leave working tree clean on completion

## Files Created

### Task Files (9 total)
- `tool-test-helpers.md` - Foundation task
- `refactor-bash-execute-test.md`
- `refactor-file-read-execute-test.md`
- `refactor-file-write-execute-test.md`
- `refactor-glob-execute-test.md`
- `refactor-grep-execute-test.md`
- `refactor-grep-edge-cases-test.md`
- `refactor-tool-limit-test.md`
- `refactor-dispatcher-test.md`

### Configuration
- `order.json` - Task queue (updated with 9 new tasks)
- `TOOL-TEST-REFACTORING.md` - This summary document

## Dependencies

Tasks are sequentially dependent:
- Task 1 must complete before tasks 2-9
- Tasks 2-9 can theoretically run in parallel, but sequential execution builds pattern confidence
- Each task depends on previous tasks leaving a clean working tree

## Notes

- This is a **pure refactoring** - no behavior changes
- All tests should pass before and after each task
- Coverage must remain at 100%
- Commit frequently during each task
- Each task is independently reversible (via git)
