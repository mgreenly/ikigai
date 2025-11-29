# Task: Execute Glob Tool

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/quality.md
- .agents/skills/mocking.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/error_handling.md
- rel-04/user-stories/02-single-glob-call.md (user story: see Tool Result A for expected format)

### Pre-read Source (patterns)
- src/config.c (JSON building with yyjson_mut, error patterns)
- src/openai/sse_parser.c (talloc and yyjson usage)
- src/json_allocator.h (talloc-based JSON allocation)
- src/error.h (res_t, OK/ERR macros)
- src/tool.h (existing tool module)

### Pre-read Tests (patterns)
- tests/unit/openai/client_structures_test.c (talloc test patterns, setup/teardown)
- tests/unit/openai/client_http_sse_streaming_test.c (mocking weak symbols, test fixtures)

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists
- Task `parse-tool-calls.md` completed

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
