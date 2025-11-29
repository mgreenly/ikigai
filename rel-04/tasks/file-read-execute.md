# Task: Execute File Read Tool

## Target
User story: 03-single-file-read

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- Task `file-read-schema.md` completed
- `ik_tool_exec_glob()` exists as reference implementation

## Context
Read before starting:
- docs/memory.md (talloc patterns)
- docs/return_values.md (Result types)
- src/tool.h (existing tool execution)
- src/tool.c (glob execution as reference)
- rel-04/user-stories/03-single-file-read.md (see Tool Result A for expected format)
- man 3 fopen, fread (file I/O functions)

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
