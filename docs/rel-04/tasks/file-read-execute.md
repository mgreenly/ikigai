# Task: Execute File Read Tool

## Target
User story: 03-single-file-read

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/testability.md
- .agents/skills/quality.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/naming.md
- rel-04/user-stories/03-single-file-read.md (user story - see Tool Result A for expected format)
- rel-04/docs/tool-result-format.md
- rel-04/docs/tool_use.md

### Pre-read Source (patterns)
- src/db/migration.c (read_file_contents function shows file I/O pattern with fopen/fread/fseek)
- src/wrapper.h (file I/O wrappers: fopen_, fread_, fclose_, fseek_, ftell_)
- src/config.c (yyjson JSON building example)
- src/error.h (res_t type definition)
- src/tool.h (existing tool execution)
- src/tool.c (glob execution as reference)

### Pre-read Tests (patterns)
- tests/unit/config/config_test.c (file I/O testing with fopen/fclose and talloc)
- tests/unit/db/zzz_migration_errors_test.c (file operation error handling patterns)

## Pre-conditions
- `make check` passes
- Task `tool-all-schemas.md` completed (provides `ik_tool_build_file_read_schema()`)
- `ik_tool_exec_glob()` exists as reference implementation

## Task
Implement file_read tool execution. Given a file path, read the file contents and return them as JSON matching the expected format:

```json
{"success": true, "data": {"output": "# My Project\n\nA simple example project."}}
```

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_file_read_execute.c`:
   - `ik_tool_exec_file_read()` with valid path returns file contents
   - Result is valid JSON with envelope format: `{"success": true, "data": {"output": "contents"}}`
   - Non-existent file returns error: `{"success": false, "error": "File not found: path"}`
   - Unreadable file (permission denied) returns error: `{"success": false, "error": "Permission denied: path"}`
   - Empty file returns `{"success": true, "data": {"output": ""}}`
2. Create test fixture files with known contents for predictable results
3. Add declaration to `src/tool.h`:
   - `ik_tool_exec_file_read(void *parent, const char *path)` returning `res_t`
   - Note: The dispatcher handles JSON parsing and extracts the path parameter
4. Add stub in `src/tool.c`: `return OK("{\"success\": true, \"data\": {\"output\": \"\"}}");` (always empty)
5. Run `make check` - expect assertion failure (tests with fixtures expect actual file contents)

### Green
1. Replace stub in `src/tool.c` with implementation:
   - Use fopen() to open the file for reading (path parameter is already extracted by dispatcher)
   - Read entire file contents (handle large files appropriately)
   - Build result JSON with yyjson in envelope format: `{"success": true, "data": {"output": "contents"}}`
   - Handle file errors (ENOENT, EACCES, etc.) and return error envelope: `{"success": false, "error": "message"}`
2. Run `make check` - expect pass

### Refactor
1. Ensure file handle is properly closed (fclose)
2. Consider memory efficiency for large files
3. Ensure proper error messages for debugging
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_exec_file_read()` reads files and returns JSON result
- Handles edge cases (missing file, empty file, permission errors)
- Tool execution dispatcher routes "file_read" calls correctly
- 100% test coverage for new code
