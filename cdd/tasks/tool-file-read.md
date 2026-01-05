# Task: File Read External Tool

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** tool-bash.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task creates the file_read external tool. Follow the same patterns established in the bash tool.

## Pre-Read

**Skills:**
- `/load errors` - Result types, PANIC usage
- `/load style` - Code style conventions

**Plan:**
- `cdd/plan/tool-specifications.md` - Section "Tool: file-read" for complete schema and behavior

**Source:**
- `src/tools/bash/main.c` - Follow same structure for --schema handling, stdin parsing, JSON output

## Libraries

Use only:
- `yyjson` (vendored) - JSON parsing and building
- `talloc` (system library) - Memory management
- POSIX file I/O - `fopen()`, `fread()`, `fseek()`, `ftell()`

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] bash tool exists at `src/tools/bash/main.c`

## Objective

Create a standalone external tool executable for reading file contents.

## Directory Structure

Create:
```
src/tools/file_read/
  main.c
```

Binary output: `libexec/ikigai/file-read`

## Schema (`--schema` output)

```json
{
  "name": "file_read",
  "description": "Read contents of a file",
  "parameters": {
    "type": "object",
    "properties": {
      "file_path": {
        "type": "string",
        "description": "Absolute or relative path to file"
      },
      "offset": {
        "type": "integer",
        "description": "Line number to start reading from (1-based)"
      },
      "limit": {
        "type": "integer",
        "description": "Number of lines to read"
      }
    },
    "required": ["file_path"]
  }
}
```

## Execution Behavior

**Success output:**
```json
{"output": "file contents here\n"}
```

**Error outputs:**

File not found:
```json
{"error": "File not found: /path/to/missing.txt", "error_code": "FILE_NOT_FOUND"}
```

Permission denied:
```json
{"error": "Permission denied: /etc/shadow", "error_code": "PERMISSION_DENIED"}
```

**Implementation details:**
- Use `fopen(file_path, "r")` to open file
- If offset/limit not provided: read entire file
- If offset provided: skip to that line (1-based)
- If limit provided: read only that many lines
- File contents preserved exactly (including newlines)
- offset beyond file end: returns empty output (not error)

## Makefile Changes

Add to Makefile (extend existing TOOL_COMMON_SRCS pattern):

```makefile
.PHONY: tool-file-read

tool-file-read: libexec/ikigai/file-read

libexec/ikigai/file-read: src/tools/file_read/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/file_read/main.c $(TOOL_COMMON_SRCS) -ltalloc
```

## Test Scenarios

1. `libexec/ikigai/file-read --schema` - Returns valid JSON schema
2. `echo '{"file_path":"/etc/hostname"}' | libexec/ikigai/file-read` - Returns file contents
3. `echo '{"file_path":"/nonexistent"}' | libexec/ikigai/file-read` - Returns FILE_NOT_FOUND error
4. `echo '{"file_path":"Makefile","offset":1,"limit":5}' | libexec/ikigai/file-read` - Returns first 5 lines

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(tool-file-read.md): [success|partial|failed] - [brief description]

[Optional: Details]
EOF
)"
```

Report status:
- Success: `/task-done tool-file-read.md`
- Partial/Failed: `/task-fail tool-file-read.md`

## Postconditions

- [ ] `make tool-file-read` builds without warnings
- [ ] `libexec/ikigai/file-read --schema` returns valid JSON
- [ ] File read with valid path works
- [ ] Missing file returns proper error JSON
- [ ] All changes committed
- [ ] Working copy is clean
