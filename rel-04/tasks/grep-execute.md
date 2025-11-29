# Task: Execute Grep Tool

## Target
User story: 05-grep-search

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- Task `grep-schema.md` completed (grep schema exists in tool-all-schemas)
- `ik_tool_exec_glob()` and `ik_tool_exec_file_read()` exist as reference implementations
- Multi-tool conversation loop works (Story 04 completed)

## Context
Read before starting:
- docs/memory.md (talloc patterns)
- docs/return_values.md (Result types)
- src/tool.h (existing tool execution)
- src/tool.c (glob and file_read execution as reference)
- rel-04/user-stories/05-grep-search.md (see Tool Result A for expected format)
- man 1 grep (for understanding grep functionality)

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
3. Run `make check` - expect compile failure

### Green
1. Add to `src/tool.h`:
   - Declare `ik_tool_exec_grep(void *parent, const char *arguments)` returning `res_t`
2. Implement in `src/tool.c`:
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
