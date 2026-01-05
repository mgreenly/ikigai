# Task: File Edit External Tool

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** tool-file-write.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task creates the file_edit external tool for text replacement in files.

## Pre-Read

**Skills:**
- `/load errors` - Result types, PANIC usage
- `/load style` - Code style conventions

**Plan:**
- `cdd/plan/tool-specifications.md` - Section "Tool: file-edit" for complete schema and behavior

**Source:**
- `src/tools/bash/main.c` - Follow same structure
- `src/tools/file_read/main.c` - File reading pattern
- `src/tools/file_write/main.c` - File writing pattern

## Libraries

Use only:
- `yyjson` (vendored) - JSON parsing and building
- `talloc` (system library) - Memory management
- POSIX file I/O
- `string.h` - `strstr()` for substring search

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)

## Objective

Create a standalone external tool executable for editing files by replacing exact text matches.

## Directory Structure

Create:
```
src/tools/file_edit/
  main.c
```

Binary output: `libexec/ikigai/file-edit`

## Schema (`--schema` output)

```json
{
  "name": "file_edit",
  "description": "Edit a file by replacing exact text matches. You must read the file before editing.",
  "parameters": {
    "type": "object",
    "properties": {
      "file_path": {
        "type": "string",
        "description": "Absolute or relative path to file"
      },
      "old_string": {
        "type": "string",
        "description": "Exact text to find and replace"
      },
      "new_string": {
        "type": "string",
        "description": "Text to replace old_string with"
      },
      "replace_all": {
        "type": "boolean",
        "description": "Replace all occurrences (default: false, fails if not unique)"
      }
    },
    "required": ["file_path", "old_string", "new_string"]
  }
}
```

## Execution Behavior

**Success output:**
```json
{"output": "Replaced 1 occurrence in config.txt", "replacements": 1}
```

**Error outputs:**

String not found:
```json
{"error": "String not found in file", "error_code": "NOT_FOUND"}
```

String not unique (multiple matches, replace_all: false):
```json
{"error": "String found 3 times, use replace_all to replace all", "error_code": "NOT_UNIQUE"}
```

No-op replacement:
```json
{"error": "old_string and new_string are identical", "error_code": "INVALID_ARG"}
```

Empty old_string:
```json
{"error": "old_string cannot be empty", "error_code": "INVALID_ARG"}
```

**Implementation details:**
- Read entire file into memory
- Search for `old_string` using exact byte-for-byte match
- If `replace_all` is false (default): require exactly one match, fail if 0 or >1
- If `replace_all` is true: replace all occurrences (0 is valid)
- Write modified content back to file atomically (write to temp, rename)
- Preserve file permissions

## Makefile Changes

Add to Makefile:

```makefile
.PHONY: tool-file-edit

tool-file-edit: libexec/ikigai/file-edit

libexec/ikigai/file-edit: src/tools/file_edit/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/file_edit/main.c $(TOOL_COMMON_SRCS) -ltalloc
```

## Test Scenarios

1. `libexec/ikigai/file-edit --schema` - Returns valid JSON schema
2. Create test file, replace unique string - Returns replacements: 1
3. Replace with replace_all: true on multiple matches - Returns correct count
4. Attempt replace on non-unique string without replace_all - Returns NOT_UNIQUE error
5. old_string == new_string - Returns INVALID_ARG error

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(tool-file-edit.md): [success|partial|failed] - [brief description]

[Optional: Details]
EOF
)"
```

Report status:
- Success: `/task-done tool-file-edit.md`
- Partial/Failed: `/task-fail tool-file-edit.md`

## Postconditions

- [ ] `make tool-file-edit` builds without warnings
- [ ] `libexec/ikigai/file-edit --schema` returns valid JSON
- [ ] Single replacement works correctly
- [ ] Multiple replacement with replace_all works
- [ ] Error cases return proper JSON
- [ ] All changes committed
- [ ] Working copy is clean
