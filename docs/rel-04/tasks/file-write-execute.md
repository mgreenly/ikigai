# Task: Execute File Write Tool

## Target
User story: 06-file-write

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/testability.md
- .agents/skills/quality.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/coverage.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/error_patterns.md
- docs/error_testing.md
- rel-04/user-stories/06-file-write.md (user story - see Tool Result A for expected format)
- rel-04/docs/tool-result-format.md
- rel-04/docs/tool_use.md

### Pre-read Source (patterns)
- src/config.c (yyjson JSON building pattern)
- src/error.h (Result type and error handling)
- src/wrapper.h (malloc/file I/O seams for testing)
- src/tool.h (existing tool execution)
- src/tool.c (glob, file_read, grep execution as reference)

### Pre-read Tests (patterns)
- tests/unit/array/basic_test.c (TDD pattern with talloc)
- tests/unit/config/config_test.c (file I/O and JSON testing pattern)

## Pre-conditions
- `make check` passes
- Task `grep-execute.md` completed (Story 05)
- `ik_tool_exec_glob()`, `ik_tool_exec_file_read()`, `ik_tool_exec_grep()` exist as reference implementations
- file_write schema exists (from tool-all-schemas)

## Task
Implement file_write tool execution. Given a file path and content, write the content to the file and return a success message as JSON matching the expected format:

```json
{"success": true, "data": {"output": "Wrote 20 bytes to notes.txt", "bytes": 20}}
```

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_file_write_execute.c`:
   - `ik_tool_exec_file_write()` with path and content writes file successfully
   - Result is valid JSON with envelope format: `{"success": true, "data": {"output": "...", "bytes": N}}`
   - Verify file was actually created with correct contents
   - Writing to read-only location returns error: `{"success": false, "error": "Permission denied: path"}`
   - Writing empty content creates empty file with `{"success": true, "data": {"output": "Wrote 0 bytes to filename", "bytes": 0}}`
   - Overwriting existing file works correctly
2. Create test fixture directory for file operations
3. Add declaration to `src/tool.h`:
   - `ik_tool_exec_file_write(void *parent, const char *path, const char *content)` returning `res_t`
   - Note: dispatcher handles JSON argument parsing and calls this with extracted parameters
4. Add stub in `src/tool.c`: `return OK("{\"success\": true, \"data\": {\"output\": \"Not implemented\", \"bytes\": 0}}");`
5. Run `make check` - expect assertion failure (tests expect actual file write and byte count)

### Green
1. Replace stub in `src/tool.c` with implementation:
   - Use fopen() to open the file for writing (mode "w")
   - Write content using fwrite() or fputs()
   - Count bytes written
   - Build result JSON with yyjson in envelope format: `{"success": true, "data": {"output": "Wrote N bytes to filename", "bytes": N}}`
   - Handle file errors (EACCES, ENOSPC, etc.) and return error envelope: `{"success": false, "error": "message"}`
   - Note: `path` and `content` parameters are already extracted by dispatcher
2. Run `make check` - expect pass

### Refactor
1. Ensure file handle is properly closed (fclose)
2. Ensure proper error messages for debugging
3. Consider atomic write patterns (write to temp, then rename) if needed
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_exec_file_write()` writes files and returns JSON result
- Handles edge cases (permission errors, empty content, overwrite)
- Tool execution dispatcher routes "file_write" calls correctly
- 100% test coverage for new code
