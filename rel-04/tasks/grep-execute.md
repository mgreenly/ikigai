# Task: Execute Grep Tool

## Target
User story: 05-grep-search

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/mocking.md
- .agents/skills/testability.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/error_handling.md
- docs/error_patterns.md
- docs/architecture.md
- rel-04/user-stories/05-grep-search.md (user story - see Tool Result A for expected format)

### Pre-read Source (patterns)
- src/config.c (JSON building with yyjson)
- src/json_allocator.c (JSON memory management)
- src/error.c (error handling patterns)
- src/wrapper.c (file I/O wrappers)
- src/tool.h (existing tool execution)
- src/tool.c (glob and file_read execution as reference)

### Pre-read Tests (patterns)
- tests/unit/error/error_test.c (Result type testing patterns)
- tests/unit/db/zzz_migration_fopen_test.c (file I/O test patterns with mocking)
- tests/integration/config_integration_test.c (yyjson testing patterns)

## Pre-conditions
- `make check` passes
- grep schema exists (from tool-all-schemas)
- `ik_tool_exec_glob()` and `ik_tool_exec_file_read()` exist as reference implementations
- Multi-tool conversation loop works (Story 04 completed)

## Task
Implement grep tool execution. Given a pattern, optional glob filter, and optional path, search for pattern matches in files and return results as JSON matching the expected format:

```json
{"output": "src/main.c:42: // TODO: add error handling\nsrc/repl.c:15: // TODO: implement history", "count": 2}
```

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_grep_execute.c`:
   - `ik_tool_exec_grep()` with pattern "TODO" in test directory returns matches
   - Result is valid JSON with "output" and "count" fields
   - Output format is "filename:line_number: matching_line"
   - Empty result returns `{"output": "", "count": 0}`
   - Invalid pattern returns error
   - Optional glob parameter filters files (e.g., "*.c" only searches C files)
   - Optional path parameter limits search to specific directory
2. Create test fixture directory with known files and content for predictable results
3. Add declaration to `src/tool.h`:
   - `ik_tool_exec_grep(void *parent, const char *arguments)` returning `res_t`
4. Add stub in `src/tool.c`: `return OK("{\"output\": \"\", \"count\": 0}");` (always empty)
5. Run `make check` - expect assertion failure (tests with fixtures expect actual grep matches)

### Green
1. Replace stub in `src/tool.c` with implementation:
   - Parse arguments JSON to extract pattern, glob (optional), path (optional)
   - Implement file search logic:
     - If glob provided, use it to filter files (integrate with glob functionality)
     - If path provided, search in that directory, else search from current directory
     - For each matching file, read and search for pattern
   - For each match, format as "filename:line_number: line_content"
   - Build result JSON with yyjson: `{"output": "...", "count": N}`
   - Handle errors (invalid pattern, path not found, etc.)
3. Update tool execution dispatcher to route "grep" to this function
4. Run `make check` - expect pass

### Refactor
1. Consider using POSIX regex (regcomp/regexec) for pattern matching
2. Ensure file handles are properly closed
3. Optimize for performance (avoid reading large binary files)
4. Consider line number tracking efficiency
5. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_exec_grep()` executes grep search and returns JSON result
- Handles edge cases (no matches, invalid pattern, path not found)
- Supports optional glob and path parameters
- Tool execution dispatcher routes "grep" calls correctly
- 100% test coverage for new code
