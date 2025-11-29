# Task: Execute File Read Tool

## Target
User story: 03-single-file-read

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/testability.md
- .agents/skills/quality.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/naming.md
- rel-04/user-stories/03-single-file-read.md (user story - see Tool Result A for expected format)

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
- Task `file-read-schema.md` completed
- `ik_tool_exec_glob()` exists as reference implementation

## Task
Implement file_read tool execution. Given a file path, read the file contents and return them as JSON matching the expected format:

```json
{"output": "# My Project\n\nA simple example project."}
```

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_file_read_execute.c`:
   - `ik_tool_exec_file_read()` with valid path returns file contents
   - Result is valid JSON with "output" field containing file contents
   - Non-existent file returns error
   - Unreadable file (permission denied) returns error
   - Empty file returns `{"output": ""}`
2. Create test fixture files with known contents for predictable results
3. Run `make check` - expect compile failure

### Green
1. Add to `src/tool.h`:
   - Declare `ik_tool_exec_file_read(void *parent, const char *arguments)` returning `res_t`
2. Implement in `src/tool.c`:
   - Parse arguments JSON to extract path
   - Use fopen() to open the file for reading
   - Read entire file contents (handle large files appropriately)
   - Build result JSON with yyjson: `{"output": "contents"}`
   - Handle file errors (ENOENT, EACCES, etc.)
3. Update tool execution dispatcher to route "file_read" to this function
4. Run `make check` - expect pass

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
