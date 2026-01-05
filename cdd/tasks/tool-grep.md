# Task: Grep External Tool

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** tool-glob.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task creates the grep external tool for regex content search.

## Pre-Read

**Skills:**
- `/load errors` - Result types, PANIC usage
- `/load style` - Code style conventions

**Plan:**
- `cdd/plan/tool-specifications.md` - Section "Tool: grep" for complete schema and behavior

**Source:**
- `src/tools/bash/main.c` - Follow same structure
- `src/tools/glob/main.c` - File enumeration pattern

## Libraries

Use only:
- `yyjson` (vendored) - JSON parsing and building
- `talloc` (system library) - Memory management
- POSIX `glob()` - File enumeration
- POSIX `regex.h` - `regcomp()`, `regexec()` for pattern matching

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)

## Objective

Create a standalone external tool executable for searching file contents using regular expressions.

## Directory Structure

Create:
```
src/tools/grep/
  main.c
```

Binary output: `libexec/ikigai/grep`

## Schema (`--schema` output)

```json
{
  "name": "grep",
  "description": "Search for pattern in files using regular expressions",
  "parameters": {
    "type": "object",
    "properties": {
      "pattern": {
        "type": "string",
        "description": "Regular expression pattern (POSIX extended)"
      },
      "glob": {
        "type": "string",
        "description": "Glob pattern to filter files (e.g., '*.c')"
      },
      "path": {
        "type": "string",
        "description": "Directory to search in (default: current directory)"
      }
    },
    "required": ["pattern"]
  }
}
```

## Execution Behavior

**Success output:**
```json
{
  "output": "src/main.c:42: // TODO: implement error handling\nsrc/util.c:15: // TODO: optimize this",
  "count": 2
}
```

No matches (not an error):
```json
{"output": "", "count": 0}
```

**Error outputs:**

Invalid regex:
```json
{"error": "Invalid pattern: Unmatched ( or \\(", "error_code": "INVALID_PATTERN"}
```

**Implementation details:**
- Use `regcomp()` with `REG_EXTENDED` for POSIX extended regex
- Use `glob()` to find files matching `path/glob` (or `path/*` if no filter)
- Skip non-regular files (directories, symlinks, devices)
- Search each file line-by-line with `getline()`
- Format matches as: `filename:line_number: line_content`
- Multiple matches separated by newlines

**Pattern syntax (POSIX ERE):**
- `.` - Any single character
- `*` - Zero or more of preceding
- `+` - One or more of preceding
- `^` - Start of line
- `$` - End of line
- `[abc]` - Character class
- `(abc|def)` - Alternation

## Makefile Changes

Add to Makefile:

```makefile
.PHONY: tool-grep

tool-grep: libexec/ikigai/grep

libexec/ikigai/grep: src/tools/grep/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/grep/main.c $(TOOL_COMMON_SRCS) -ltalloc
```

## Test Scenarios

1. `libexec/ikigai/grep --schema` - Returns valid JSON schema
2. `echo '{"pattern":"main","glob":"*.c","path":"src"}' | libexec/ikigai/grep` - Finds main in .c files
3. `echo '{"pattern":"nonexistent_string"}' | libexec/ikigai/grep` - Returns count: 0
4. `echo '{"pattern":"[invalid"}' | libexec/ikigai/grep` - Returns INVALID_PATTERN error

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(tool-grep.md): [success|partial|failed] - [brief description]

[Optional: Details]
EOF
)"
```

Report status:
- Success: `/task-done tool-grep.md`
- Partial/Failed: `/task-fail tool-grep.md`

## Postconditions

- [ ] `make tool-grep` builds without warnings
- [ ] `libexec/ikigai/grep --schema` returns valid JSON
- [ ] Regex search returns correct matches with file:line format
- [ ] Invalid regex returns proper error
- [ ] All changes committed
- [ ] Working copy is clean
