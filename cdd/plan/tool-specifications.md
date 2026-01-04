# External Tool Specifications

Complete schemas, execution behavior, and error handling for the 6 built-in external tools.

## Tool: bash

Execute shell commands via `/bin/sh`.

### Schema (`--schema` output)

```json
{
  "name": "bash",
  "description": "Execute a shell command and return output",
  "parameters": {
    "type": "object",
    "properties": {
      "command": {
        "type": "string",
        "description": "Shell command to execute"
      }
    },
    "required": ["command"]
  }
}
```

### Execution Behavior

**Input (stdin):**
```json
{
  "command": "ls -la /tmp"
}
```

**Output (stdout):**
```json
{
  "output": "total 8\ndrwxrwxrwt  10 root root  220 Dec 24 10:00 .\ndrwxr-xr-x  20 root root 4096 Dec 20 08:00 ..",
  "exit_code": 0
}
```

**Implementation details:**
- Use `popen(command, "r")` to execute command
- Capture stdout only (stderr is merged with stdout by shell)
- Strip single trailing newline from output (if present)
- Dynamic buffer allocation (start 4KB, grow as needed)
- Exit code extracted via `WEXITSTATUS(pclose(fp))`
- Failed `popen()` treated as exit code 127

**Edge cases:**
- Empty command: shell error (exit code 127)
- Command not found: shell error (exit code 127)
- Command timeout: not handled by tool (ikigai wrapper handles)
- Multi-line output: preserved with `\n` separators
- Binary output: may contain null bytes (JSON escaping required)

### Error Cases

**Tool never returns error envelope.** All execution outcomes (success, command failure, shell errors) use success envelope with appropriate exit codes.

**Exit code meanings:**
- `0` - Command succeeded
- `1-126` - Command failed with specific error code
- `127` - Command not found or shell error
- `128+N` - Command terminated by signal N (e.g., 130 = Ctrl+C)

**Examples:**

Command not found:
```json
{
  "output": "sh: 1: nonexistent: not found",
  "exit_code": 127
}
```

Command failed:
```json
{
  "output": "ls: cannot access '/nonexistent': No such file or directory",
  "exit_code": 2
}
```

## Tool: file-read

Read file contents from filesystem.

### Schema (`--schema` output)

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

### Execution Behavior

**Input (stdin):**
```json
{
  "file_path": "/etc/hostname"
}
```

**Output (stdout):**
```json
{
  "output": "myserver.example.com\n"
}
```

**Implementation details:**
- Use `fopen(file_path, "r")` to open file
- If `offset` and `limit` not provided: read entire file
- If `offset` provided: skip to that line (1-based)
- If `limit` provided: read only that many lines
- Use line-by-line reading when offset/limit specified
- File contents preserved exactly (including newlines)
- No size limit enforced by tool (ikigai may impose limits)

**Edge cases:**
- Empty file: returns `{"output": ""}`
- Binary file: raw bytes returned (JSON escaping required)
- Symbolic links: followed automatically by `fopen()`
- Relative paths: resolved relative to tool's working directory
- Large files: entire file read into memory (may fail on very large files)
- `offset` beyond file end: returns empty output (not an error)
- `offset` without `limit`: reads from offset to end of file

### Error Cases

Tool returns error envelope for filesystem failures:

**File not found (ENOENT):**
```json
{
  "error": "File not found: /path/to/missing.txt",
  "error_code": "FILE_NOT_FOUND"
}
```

**Permission denied (EACCES):**
```json
{
  "error": "Permission denied: /etc/shadow",
  "error_code": "PERMISSION_DENIED"
}
```

**Cannot open file (other errors):**
```json
{
  "error": "Cannot open file: /path/to/file",
  "error_code": "OPEN_FAILED"
}
```

**Cannot seek file:**
```json
{
  "error": "Cannot seek file: /dev/stdin",
  "error_code": "SEEK_FAILED"
}
```

**Cannot get file size:**
```json
{
  "error": "Cannot get file size: /dev/zero",
  "error_code": "SIZE_FAILED"
}
```

**Failed to read file:**
```json
{
  "error": "Failed to read file: /path/to/file",
  "error_code": "READ_FAILED"
}
```

## Tool: file-write

Write content to filesystem.

### Schema (`--schema` output)

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

### Execution Behavior

**Input (stdin):**
```json
{
  "file_path": "/tmp/test.txt",
  "content": "Hello, world!\n"
}
```

**Output (stdout):**
```json
{
  "output": "Wrote 14 bytes to test.txt",
  "bytes": 14
}
```

**Implementation details:**
- Use `fopen(path, "w")` to open file (creates or truncates)
- Use `fwrite(content, 1, content_len, fp)` to write content
- Success message uses `basename(path)` for filename only
- Empty content (`""`) is valid - creates empty file
- File permissions: default creation mode (typically 0644)

**Edge cases:**
- Empty content: creates 0-byte file, returns `{"output": "Wrote 0 bytes to ...", "bytes": 0}`
- Existing file: truncated and overwritten
- Parent directory must exist (tool does not create directories)
- Relative paths: resolved relative to tool's working directory
- Binary content: written as-is (raw bytes)

### Error Cases

Tool returns error envelope for filesystem failures:

**Permission denied (EACCES):**
```json
{
  "error": "Permission denied: /etc/hosts",
  "error_code": "PERMISSION_DENIED"
}
```

**No space left on device (ENOSPC):**
```json
{
  "error": "No space left on device: /tmp/test.txt",
  "error_code": "NO_SPACE"
}
```

**Cannot open file (other errors):**
```json
{
  "error": "Cannot open file: /nonexistent/dir/file.txt",
  "error_code": "OPEN_FAILED"
}
```

**Failed to write file:**
```json
{
  "error": "Failed to write file: /tmp/test.txt",
  "error_code": "WRITE_FAILED"
}
```

**Note:** Partial writes (where `fwrite()` returns less than requested) are treated as write failures.

## Tool: file-edit

Edit file by replacing exact text matches.

### Schema (`--schema` output)

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

### Execution Behavior

**Input (stdin):**
```json
{
  "file_path": "/tmp/config.txt",
  "old_string": "debug = false",
  "new_string": "debug = true"
}
```

**Output (stdout):**
```json
{
  "output": "Replaced 1 occurrence in config.txt",
  "replacements": 1
}
```

**Implementation details:**
- Description says "must read before editing" but this is NOT enforced (encourages LLM to read first)
- Read entire file into memory
- Search for `old_string` using exact byte-for-byte match
- If `replace_all` is false (default): require exactly one match, fail if 0 or >1
- If `replace_all` is true: replace all occurrences (0 is valid)
- Write modified content back to file atomically (write to temp, rename)
- Preserve file permissions and ownership

**Edge cases:**
- `old_string` equals `new_string`: error (no-op not allowed)
- Empty `old_string`: error (invalid argument)
- Empty `new_string`: valid (deletes matched text)
- No matches with `replace_all: false`: error (not unique)
- No matches with `replace_all: true`: success with `replacements: 0`
- Binary files: works on raw bytes
- Relative paths: resolved relative to tool's working directory

### Error Cases

Tool returns error envelope for edit failures:

**File not found (ENOENT):**
```json
{
  "error": "File not found: /path/to/missing.txt",
  "error_code": "FILE_NOT_FOUND"
}
```

**Permission denied (EACCES):**
```json
{
  "error": "Permission denied: /etc/passwd",
  "error_code": "PERMISSION_DENIED"
}
```

**String not found:**
```json
{
  "error": "String not found in file",
  "error_code": "NOT_FOUND"
}
```

**String not unique (multiple matches, replace_all: false):**
```json
{
  "error": "String found 3 times, use replace_all to replace all",
  "error_code": "NOT_UNIQUE"
}
```

**No-op replacement:**
```json
{
  "error": "old_string and new_string are identical",
  "error_code": "INVALID_ARG"
}
```

**Empty old_string:**
```json
{
  "error": "old_string cannot be empty",
  "error_code": "INVALID_ARG"
}
```

## Tool: glob

Find files matching pattern.

### Schema (`--schema` output)

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

### Execution Behavior

**Input (stdin):**
```json
{
  "pattern": "*.txt",
  "path": "/tmp"
}
```

**Output (stdout):**
```json
{
  "output": "/tmp/file1.txt\n/tmp/file2.txt\n/tmp/notes.txt",
  "count": 3
}
```

**Implementation details:**
- Use POSIX `glob()` function with pattern
- Construct full pattern: `path/pattern` or just `pattern` if path empty
- Results sorted by `glob()` (typically lexicographic)
- Output format: one path per line, no trailing newline after last path
- No matches: returns `{"output": "", "count": 0}`

**Pattern syntax (POSIX glob):**
- `*` - Matches any string (including empty)
- `?` - Matches any single character
- `[abc]` - Matches any character in set
- `[a-z]` - Matches any character in range
- `**` - Not supported (POSIX glob has no recursive wildcard)

**Edge cases:**
- No matches: `{"output": "", "count": 0}` (not an error)
- Single match: `{"output": "/path/to/file", "count": 1}` (no newline)
- Pattern with no wildcards: exact match if exists
- Relative paths: resolved relative to tool's working directory
- Dotfiles: not matched by `*` (POSIX glob behavior)

### Error Cases

Tool returns error envelope for glob failures:

**Out of memory (GLOB_NOSPACE):**
```json
{
  "error": "Out of memory during glob",
  "error_code": "OUT_OF_MEMORY"
}
```

**Read error (GLOB_ABORTED):**
```json
{
  "error": "Read error during glob",
  "error_code": "READ_ERROR"
}
```

**Invalid pattern:**
```json
{
  "error": "Invalid glob pattern",
  "error_code": "INVALID_PATTERN"
}
```

**Note:** `GLOB_NOMATCH` (no matches) is not an error - returns success envelope with count 0.

## Tool: grep

Search file contents using regular expressions.

### Schema (`--schema` output)

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

### Execution Behavior

**Input (stdin):**
```json
{
  "pattern": "TODO",
  "glob": "*.c",
  "path": "src"
}
```

**Output (stdout):**
```json
{
  "output": "src/main.c:42: // TODO: implement error handling\nsrc/util.c:15: // TODO: optimize this",
  "count": 2
}
```

**Implementation details:**
- Use `regcomp()` with `REG_EXTENDED` for POSIX extended regex
- Use `glob()` to find files matching `path/glob` (or `path/*` if no filter)
- Skip non-regular files (directories, symlinks, devices)
- Search each file line-by-line with `getline()`
- Use `regexec()` to test each line against pattern
- Format matches as: `filename:line_number: line_content`
- Multiple matches separated by newlines
- No matches: returns `{"output": "", "count": 0}`

**Pattern syntax (POSIX ERE):**
- `.` - Any single character
- `*` - Zero or more of preceding
- `+` - One or more of preceding
- `?` - Zero or one of preceding
- `^` - Start of line
- `$` - End of line
- `[abc]` - Character class
- `(abc|def)` - Alternation
- `\` - Escape special characters

**Edge cases:**
- No matches: `{"output": "", "count": 0}` (not an error)
- Empty glob: searches all files in path
- Empty path: defaults to current directory (`.`)
- Files that cannot be opened: silently skipped
- Binary files: searched as text (may produce garbage matches)
- Very long lines: entire line returned in match

### Error Cases

Tool returns error envelope for regex compilation failures only:

**Invalid regular expression:**
```json
{
  "error": "Invalid pattern: Unmatched ( or \\(",
  "error_code": "INVALID_PATTERN"
}
```

**Note:** Other errors (glob failures, file read failures) are silently ignored - tool continues searching remaining files. Only regex compilation errors cause tool to abort.

## Common Patterns

### Tool Naming Convention

- **Source directory:** `src/tools/file_read/` (underscore)
- **Binary name:** `libexec/ikigai/file-read` (hyphen)
- **Tool name in schema:** `file_read` (underscore)
- **LLM sees:** `file_read` (underscore)
- **Make target:** `tool-file-read` (hyphen)

### JSON I/O Protocol

All tools follow this protocol:

1. **Input:** Read JSON from stdin until EOF
2. **Parse:** Validate JSON and extract parameters
3. **Execute:** Perform operation
4. **Output:** Write JSON to stdout (single object, no trailing newline)
5. **Exit:** Exit with code 0 (success or operation failure both use exit 0)

### Response Envelope Wrapping

Tools return simple JSON. ikigai wraps responses before sending to LLM:

**Tool success:**
```json
{
  "tool_success": true,
  "result": {
    ... tool's JSON output ...
  }
}
```

**Tool failure (crash, timeout, invalid JSON):**
```json
{
  "tool_success": false,
  "error": "Tool 'bash' crashed with exit code 139",
  "error_code": "TOOL_CRASHED",
  "exit_code": 139,
  "stdout": "partial output...",
  "stderr": "segmentation fault"
}
```

### Error Codes

**ikigai wrapper codes** (when `tool_success: false`):
- `TOOL_TIMEOUT` - Tool exceeded 30s execution timeout
- `TOOL_CRASHED` - Tool exited with non-zero code
- `INVALID_OUTPUT` - Tool returned malformed JSON
- `TOOL_NOT_FOUND` - Tool not in registry
- `INVALID_PARAMS` - ikigai failed to marshal parameters (should not happen)

**Tool-specific codes** (in tool's JSON output):
- `FILE_NOT_FOUND` - File does not exist
- `PERMISSION_DENIED` - Insufficient permissions
- `OPEN_FAILED` - Cannot open file/resource
- `READ_FAILED` - Read operation failed
- `WRITE_FAILED` - Write operation failed
- `SEEK_FAILED` - File seek failed
- `SIZE_FAILED` - Cannot determine file size
- `NO_SPACE` - Disk full
- `INVALID_PATTERN` - Invalid regex/glob pattern
- `OUT_OF_MEMORY` - Memory allocation failed
- `READ_ERROR` - Filesystem read error

### Memory Management

External tools manage their own memory. Recommendations:

- Use talloc for memory management (matches ikigai conventions)
- Parent all allocations to single root context
- Free root context on exit (or rely on process termination cleanup)
- Handle allocation failures gracefully (return error envelope)

### Security Considerations

- **Path traversal:** Tools do not validate paths - filesystem permissions are security boundary
- **Command injection:** bash tool passes command directly to shell - caller must sanitize
- **Resource limits:** Tools do not enforce limits - ikigai wrapper provides timeout
- **Symlink handling:** Tools follow symlinks - filesystem permissions apply to target

### Testing

Each tool should have:
- Unit tests for success cases
- Unit tests for error cases (file not found, permission denied, etc.)
- Integration tests with ikigai wrapper
- Edge case tests (empty input, very large files, binary data, etc.)

### Build System

**Directory Structure:**
```
src/tools/
  bash/
    main.c
  file_read/
    main.c
  file_write/
    main.c
  file_edit/
    main.c
  glob/
    main.c
  grep/
    main.c
```

Each tool is self-contained in its directory. All source files for a tool live in that directory.

**Shared Sources:**

Tools link directly against these shared source files (no static library):
```
src/error.c          # res_t, OK/ERR macros
src/panic.c          # PANIC macro
src/json_allocator.c # talloc-based yyjson allocator
src/vendor/yyjson/yyjson.c
```

**Output Location:**
```
libexec/ikigai/
  bash
  file-read
  file-write
  file-edit
  glob
  grep
```

Binary names use hyphens. Installed to `PREFIX/libexec/ikigai/` (not in `PATH`).

**Make Targets:**

```makefile
# Shared sources for all tools
TOOL_COMMON_SRCS = src/error.c src/panic.c src/json_allocator.c src/vendor/yyjson/yyjson.c

# All tools
TOOLS = libexec/ikigai/bash libexec/ikigai/file-read libexec/ikigai/file-write \
        libexec/ikigai/file-edit libexec/ikigai/glob libexec/ikigai/grep

.PHONY: tools tool-bash tool-file-read tool-file-write tool-file-edit tool-glob tool-grep

tools: $(TOOLS)

tool-bash: libexec/ikigai/bash
tool-file-read: libexec/ikigai/file-read
tool-file-write: libexec/ikigai/file-write
tool-file-edit: libexec/ikigai/file-edit
tool-glob: libexec/ikigai/glob
tool-grep: libexec/ikigai/grep

# Individual tool builds
libexec/ikigai/bash: src/tools/bash/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/bash/main.c $(TOOL_COMMON_SRCS) -ltalloc

libexec/ikigai/file-read: src/tools/file_read/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/file_read/main.c $(TOOL_COMMON_SRCS) -ltalloc

libexec/ikigai/file-write: src/tools/file_write/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/file_write/main.c $(TOOL_COMMON_SRCS) -ltalloc

libexec/ikigai/file-edit: src/tools/file_edit/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/file_edit/main.c $(TOOL_COMMON_SRCS) -ltalloc

libexec/ikigai/glob: src/tools/glob/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/glob/main.c $(TOOL_COMMON_SRCS) -ltalloc

libexec/ikigai/grep: src/tools/grep/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/grep/main.c $(TOOL_COMMON_SRCS) -ltalloc

libexec/ikigai:
	mkdir -p $@
```

**Naming Convention:**
- Source directory: `src/tools/file_read/` (underscore)
- Binary name: `libexec/ikigai/file-read` (hyphen)
- Tool name in schema: `file_read` (underscore)
- Make target: `tool-file-read` (hyphen)
