# Task: File Write External Tool

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** tool-file-read.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task creates the file_write external tool.

## Pre-Read

**Skills:**
- `/load errors` - Result types, PANIC usage
- `/load style` - Code style conventions

**Plan:**
- `cdd/plan/tool-specifications.md` - Section "Tool: file-write" for complete schema and behavior

**Source:**
- `src/tools/bash/main.c` - Follow same structure
- `src/tools/file_read/main.c` - Similar file operations

## Libraries

Use only:
- `yyjson` (vendored) - JSON parsing and building
- `talloc` (system library) - Memory management
- POSIX file I/O - `fopen()`, `fwrite()`
- `libgen.h` - `basename()` for filename extraction

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)

## Objective

Create a standalone external tool executable for writing file contents.

## Directory Structure

Create:
```
src/tools/file_write/
  main.c
```

Binary output: `libexec/ikigai/file-write`

## Schema (`--schema` output)

```json
{
  "name": "file_write",
  "description": "Write content to a file (creates or overwrites)",
  "parameters": {
    "type": "object",
    "properties": {
      "file_path": {
        "type": "string",
        "description": "Absolute or relative path to file"
      },
      "content": {
        "type": "string",
        "description": "Content to write to file"
      }
    },
    "required": ["file_path", "content"]
  }
}
```

## Execution Behavior

**Success output:**
```json
{"output": "Wrote 14 bytes to test.txt", "bytes": 14}
```

**Error outputs:**

Permission denied:
```json
{"error": "Permission denied: /etc/hosts", "error_code": "PERMISSION_DENIED"}
```

No space:
```json
{"error": "No space left on device: /tmp/test.txt", "error_code": "NO_SPACE"}
```

Cannot open:
```json
{"error": "Cannot open file: /nonexistent/dir/file.txt", "error_code": "OPEN_FAILED"}
```

**Implementation details:**
- Use `fopen(path, "w")` to open file (creates or truncates)
- Use `fwrite(content, 1, content_len, fp)` to write
- Success message uses `basename(path)` for filename only
- Empty content is valid - creates 0-byte file
- Parent directory must exist (tool does not create directories)

## Makefile Changes

Add to Makefile:

```makefile
.PHONY: tool-file-write

tool-file-write: libexec/ikigai/file-write

libexec/ikigai/file-write: src/tools/file_write/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/file_write/main.c $(TOOL_COMMON_SRCS) -ltalloc
```

## Test Scenarios

1. `libexec/ikigai/file-write --schema` - Returns valid JSON schema
2. `echo '{"file_path":"/tmp/test.txt","content":"hello"}' | libexec/ikigai/file-write` - Writes file, returns byte count
3. `echo '{"file_path":"/etc/passwd","content":"x"}' | libexec/ikigai/file-write` - Returns PERMISSION_DENIED
4. `echo '{"file_path":"/nonexistent/x","content":"x"}' | libexec/ikigai/file-write` - Returns OPEN_FAILED

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(tool-file-write.md): [success|partial|failed] - [brief description]

[Optional: Details]
EOF
)"
```

Report status:
- Success: `/task-done tool-file-write.md`
- Partial/Failed: `/task-fail tool-file-write.md`

## Postconditions

- [ ] `make tool-file-write` builds without warnings
- [ ] `libexec/ikigai/file-write --schema` returns valid JSON
- [ ] File write creates file with correct content
- [ ] Permission errors return proper JSON
- [ ] All changes committed
- [ ] Working copy is clean
