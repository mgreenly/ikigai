# External Tools Implementation Plan

Implement the remaining 5 external tools: file_read, file_write, file_edit, glob, grep.

## Reference

- `rel-08/plan/tool-specifications.md` - Detailed schemas, behavior, error cases
- `rel-08/plan/tool-discovery-execution.md` - Discovery and execution protocol
- `src/tools/bash/main.c` - Reference implementation
- `tests/integration/bash_tool_test.sh` - Test pattern

## Pattern from bash_tool

Each tool follows this structure:

```c
int32_t main(int32_t argc, char **argv) {
    void *ctx = talloc_new(NULL);

    // 1. Handle --schema flag
    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        // Print JSON schema to stdout
        return 0;
    }

    // 2. Read stdin into buffer (dynamic growth)
    // 3. Parse JSON input with yyjson
    // 4. Validate required fields
    // 5. Execute operation
    // 6. Build JSON result with yyjson
    // 7. Print JSON to stdout
    // 8. Return 0 (always, even for errors)

    talloc_free(ctx);
    return 0;
}
```

**Key conventions:**
- Exit 0 always (errors returned in JSON)
- Use talloc for all allocations
- Use yyjson for JSON parsing/writing (handles escaping)
- Error input (empty, invalid JSON, missing fields) → stderr + exit 1
- Operation errors (file not found, etc.) → JSON error envelope

## Naming Convention

| Aspect | Format | Example |
|--------|--------|---------|
| Source dir | underscore | `src/tools/file_read/` |
| Binary | hyphen | `libexec/ikigai/file-read` |
| Schema name | underscore | `"name": "file_read"` |
| Make target | hyphen | `tool-file-read` |

## System Requirements

### Resource Limits
- Maximum file path length: 4096 bytes
- Maximum file size for read/write operations: 100MB
- Maximum regex pattern length: 1024 characters
- Maximum file content size for file_edit: 100MB (must fit in memory)

### Path Validation
- All file paths undergo canonical path resolution to prevent directory traversal
- Paths containing `../` components are rejected
- Symlinks are followed (standard POSIX behavior)
- Absolute and relative paths are supported

### File Operations
- Tools inherit process umask for file creation
- Temporary files are cleaned up on SIGTERM/SIGINT via atexit handlers
- file_edit uses open() then fstat() on file descriptor to prevent TOCTOU races
- Atomic writes (file_edit) use temp file + rename pattern

### Error Message Format
All error messages follow the format: `Error: {operation} '{path}': {reason}`

Example: `Error: reading file '/etc/shadow': Permission denied`

## Tools to Implement

### 1. file_read

**Schema:** See tool-specifications.md

**Parameters:**
- `file_path` (required) - Path to file
- `offset` (optional) - Line number to start (1-based)
- `limit` (optional) - Number of lines

**Success response:**
```json
{"output": "file contents..."}
```

**Error responses:**

| Error Code | Condition | User Message |
|------------|-----------|--------------|
| `FILE_NOT_FOUND` | ENOENT | File does not exist |
| `PERMISSION_DENIED` | EACCES | Permission denied |
| `OPEN_FAILED` | Other fopen errors | Cannot open file |
| `SEEK_FAILED` | fseek failed | Cannot seek in file |
| `SIZE_FAILED` | fstat failed | Cannot determine file size |
| `READ_FAILED` | fread failed | Cannot read file |

**Implementation notes:**
- Use fopen/fread for whole file
- Use getline() for offset/limit (line-by-line)
- offset beyond EOF → empty output (not error)
- Preserve file contents exactly

### 2. file_write

**Parameters:**
- `file_path` (required) - Path to file
- `content` (required) - Content to write

**Success response:**
```json
{"output": "Wrote 14 bytes to test.txt", "bytes": 14}
```

**Error responses:**

| Error Code | Condition | User Message |
|------------|-----------|--------------|
| `PERMISSION_DENIED` | EACCES | Permission denied |
| `NO_SPACE` | ENOSPC | No space left on device |
| `OPEN_FAILED` | Other fopen errors | Cannot open file |
| `WRITE_FAILED` | fwrite failed/partial | Cannot write to file |

**Implementation notes:**
- Use fopen with "w" (truncate)
- Empty content valid → 0-byte file
- Parent directory must exist
- Use basename() for success message

### 3. file_edit

**Parameters:**
- `file_path` (required) - Path to file
- `old_string` (required) - Text to find
- `new_string` (required) - Replacement text
- `replace_all` (optional, default false)

**Success response:**
```json
{"output": "Replaced 1 occurrence in config.txt", "replacements": 1}
```

**Error responses:**

| Error Code | Condition | User Message |
|------------|-----------|--------------|
| `FILE_NOT_FOUND` | ENOENT | File does not exist |
| `PERMISSION_DENIED` | EACCES | Permission denied |
| `NOT_FOUND` | old_string not in file (replace_all: false) | String not found in file |
| `NOT_UNIQUE` | Multiple matches (replace_all: false) | String appears multiple times (use replace_all) |
| `INVALID_ARG` | old_string empty or equals new_string | Invalid replacement parameters |

**Implementation notes:**
- Read file into memory
- Count occurrences first
- If replace_all: false, require exactly 1 match
- If replace_all: true, 0 matches is success (replacements: 0)
- Write to temp file, rename (atomic)

### 4. glob

**Parameters:**
- `pattern` (required) - Glob pattern (e.g., `*.txt`)
- `path` (optional) - Directory to search

**Success response:**
```json
{"output": "/tmp/a.txt\n/tmp/b.txt", "count": 2}
```

**Error responses:**

| Error Code | Condition | User Message |
|------------|-----------|--------------|
| `OUT_OF_MEMORY` | GLOB_NOSPACE | Out of memory |
| `READ_ERROR` | GLOB_ABORTED | Cannot read directory |
| `INVALID_PATTERN` | Invalid pattern | Invalid glob pattern |

**Implementation notes:**
- Use POSIX glob()
- Construct pattern: `path/pattern` or just `pattern`
- GLOB_NOMATCH → success with count: 0
- Output: one path per line, no trailing newline
- No `**` (recursive) - POSIX glob doesn't support

### 5. grep

**Parameters:**
- `pattern` (required) - POSIX ERE regex
- `glob` (optional) - File filter pattern
- `path` (optional) - Directory to search

**Success response:**
```json
{"output": "src/main.c:42: // TODO: fix", "count": 1}
```

**Error responses:**

| Error Code | Condition | User Message |
|------------|-----------|--------------|
| `INVALID_PATTERN` | regcomp() failed | Invalid regular expression |

**Implementation notes:**
- Use regcomp() with REG_EXTENDED
- Use glob() to find files
- Skip non-regular files
- Read files line-by-line with getline()
- regexec() each line
- Output format: `filename:line_number: line_content`
- Silently skip files that can't be opened
- No matches → success with count: 0

## Implementation Order

Order by complexity (simplest first):

1. **file_write** - Simplest (fopen, fwrite, done)
2. **file_read** - Simple, but offset/limit adds complexity
3. **glob** - POSIX glob() wrapper
4. **grep** - Combines regex + glob + file reading
5. **file_edit** - Most complex (find, replace, atomic write)

## Makefile Updates

Add for each tool:

```makefile
tool-file-read: libexec/ikigai/file-read
tool-file-write: libexec/ikigai/file-write
tool-file-edit: libexec/ikigai/file-edit
tool-glob: libexec/ikigai/glob
tool-grep: libexec/ikigai/grep

libexec/ikigai/file-read: src/tools/file_read/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ltalloc && echo "🔗 $@" || (echo "🔴 $@" && exit 1)

# ... similar for others
```

Add to `.PHONY` and update `tools` target.

## Testing

Each tool gets an integration test following bash_tool_test.sh pattern:

```bash
tests/integration/file_read_test.sh
tests/integration/file_write_test.sh
tests/integration/file_edit_test.sh
tests/integration/glob_test.sh
tests/integration/grep_test.sh
```

**Test categories:**
1. `--schema` exits 0, outputs valid JSON with required fields
2. Success cases with various inputs
3. Error cases (file not found, permission denied, etc.)
4. Edge cases (empty input, large files, special characters)

## Acceptance

For each tool:
1. `--schema` outputs valid JSON matching tool-specifications.md
2. All integration tests pass
3. `make check-build` passes
4. Tool handles all error cases per specification
5. All error messages match the specified user messages in error response tables
6. All error codes defined in specifications have integration tests that trigger them
7. Test suite passes under valgrind with zero memory leaks or errors
8. file_edit atomicity test verifies no corruption after simulated crash during write
