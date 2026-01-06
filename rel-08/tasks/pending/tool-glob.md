# Task: Glob External Tool

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** tool-file-edit.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task creates the glob external tool for file pattern matching.

## Pre-Read

**Skills:**
- `/load errors` - Result types, PANIC usage
- `/load style` - Code style conventions

**Plan:**
- `cdd/plan/tool-specifications.md` - Section "Tool: glob" for complete schema and behavior

**Source:**
- `src/tools/bash/main.c` - Follow same structure

## Libraries

Use only:
- `yyjson` (vendored) - JSON parsing and building
- `talloc` (system library) - Memory management
- POSIX `glob()` - Pattern matching

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)

## Objective

Create a standalone external tool executable for finding files matching glob patterns.

## Directory Structure

Create:
```
src/tools/glob/
  main.c
```

Binary output: `libexec/ikigai/glob`

## Schema (`--schema` output)

```json
{
  "name": "glob",
  "description": "Find files matching a glob pattern",
  "parameters": {
    "type": "object",
    "properties": {
      "pattern": {
        "type": "string",
        "description": "Glob pattern (e.g., '*.txt', 'src/**/*.c')"
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
{"output": "/tmp/file1.txt\n/tmp/file2.txt\n/tmp/notes.txt", "count": 3}
```

No matches (not an error):
```json
{"output": "", "count": 0}
```

**Error outputs:**

Out of memory:
```json
{"error": "Out of memory during glob", "error_code": "OUT_OF_MEMORY"}
```

Read error:
```json
{"error": "Read error during glob", "error_code": "READ_ERROR"}
```

**Implementation details:**
- Use POSIX `glob()` function with pattern
- Construct full pattern: `path/pattern` or just `pattern` if path empty
- Results sorted by `glob()` (typically lexicographic)
- Output format: one path per line, no trailing newline after last path
- No matches: returns success with count 0

**Pattern syntax (POSIX glob):**
- `*` - Matches any string (including empty)
- `?` - Matches any single character
- `[abc]` - Matches any character in set
- `**` - Not supported by POSIX glob

## Makefile Changes

Add to Makefile:

```makefile
.PHONY: tool-glob

tool-glob: libexec/ikigai/glob

libexec/ikigai/glob: src/tools/glob/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/glob/main.c $(TOOL_COMMON_SRCS) -ltalloc
```

## Test Specification

**Reference:** `cdd/plan/test-specification.md` → "Phase 1: External Tools" → "tool-glob.md"

**Testing approach:** Shell-based manual verification.

**Manual verification commands:**
1. `libexec/ikigai/glob --schema` - Returns valid JSON schema
2. `echo '{"pattern":"*.c","path":"src"}' | libexec/ikigai/glob` - Returns list of .c files
3. `echo '{"pattern":"nonexistent*"}' | libexec/ikigai/glob` - Returns count:0 (success, not error)
4. `echo '{"pattern":"*"}' | libexec/ikigai/glob` - Lists current directory

**Behaviors to verify:**
- Pattern matching returns sorted list (lexicographic from glob())
- No matches returns count:0 with success envelope
- Output format: one path per line, no trailing newline after last
- Path parameter prepended to pattern correctly
- GLOB_NOSPACE returns OUT_OF_MEMORY error
- GLOB_ABORTED returns READ_ERROR error

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(tool-glob.md): [success|partial|failed] - [brief description]

[Optional: Details]
EOF
)"
```

Report status:
- Success: `/task-done tool-glob.md`
- Partial/Failed: `/task-fail tool-glob.md`

## Postconditions

- [ ] `make tool-glob` builds without warnings
- [ ] `libexec/ikigai/glob --schema` returns valid JSON
- [ ] Pattern matching returns correct files
- [ ] No matches returns count: 0
- [ ] All changes committed
- [ ] Working copy is clean
