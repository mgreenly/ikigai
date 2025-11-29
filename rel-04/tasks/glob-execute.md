# Task: Execute Glob Tool

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists
- Task `parse-tool-calls.md` completed

## Context
Read before starting:
- docs/memory.md (talloc patterns)
- docs/return_values.md (Result types)
- src/tool.h (existing tool module)
- rel-04/user-stories/02-single-glob-call.md (see Tool Result A for expected format)
- man 3 glob (POSIX glob function)

## Task
Implement glob tool execution. Given a pattern and optional path, execute glob and return results as JSON matching the expected format:

```json
{"output": "src/main.c\nsrc/config.c\nsrc/repl.c", "count": 3}
```

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_glob_execute.c`:
   - `ik_tool_exec_glob()` with pattern "*.c" in test directory returns matches
   - Result is valid JSON with "output" and "count" fields
   - Empty result returns `{"output": "", "count": 0}`
   - Invalid pattern returns error
2. Create a test fixture directory with known files for predictable results
3. Run `make check` - expect compile failure

### Green
1. Add to `src/tool.h`:
   - Declare `ik_tool_exec_glob(void *parent, const char *pattern, const char *path)` returning `res_t`
2. Implement in `src/tool.c`:
   - Parse arguments JSON to extract pattern and path
   - Use POSIX glob() to find matching files
   - Build result JSON with yyjson
   - Handle glob errors (GLOB_NOMATCH, GLOB_NOSPACE, etc.)
3. Run `make check` - expect pass

### Refactor
1. Ensure glob() resources are properly freed (globfree)
2. Consider path validation (prevent directory traversal if needed)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_exec_glob()` executes glob and returns JSON result
- Handles edge cases (no matches, invalid pattern)
- 100% test coverage for new code
