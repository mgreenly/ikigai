# Tool Result Format

All tool execution results use a consistent JSON envelope format.

## Success

When a tool executes successfully:

```json
{"success": true, "data": {...tool-specific fields...}}
```

## Error

When a tool encounters an operational error (file not found, permission denied, invalid input):

```json
{"success": false, "error": "Human-readable error message"}
```

## Tool-Specific Data Fields

### glob

```json
{"success": true, "data": {"output": "file1.c\nfile2.c", "count": 2}}
```

- `output`: Newline-separated list of matching files
- `count`: Number of matches

### file_read

```json
{"success": true, "data": {"output": "file contents here"}}
```

- `output`: File contents as string

### grep

```json
{"success": true, "data": {"output": "file.c:42: matching line", "count": 1}}
```

- `output`: Newline-separated matches in `filename:line: content` format
- `count`: Number of matches

### file_write

```json
{"success": true, "data": {"output": "Wrote 42 bytes to filename.txt", "bytes": 42}}
```

- `output`: Human-readable confirmation message
- `bytes`: Number of bytes written

### bash

```json
{"success": true, "data": {"output": "command output", "exit_code": 0}}
```

- `output`: Combined stdout/stderr from command
- `exit_code`: Process exit code (0 = success, non-zero = failure)

**Note**: A bash command that runs but returns non-zero exit code is still `"success": true` - the tool executed successfully. The `exit_code` indicates the command's result. Use `"success": false` only when the tool itself fails (e.g., popen fails).

## Error Examples

```json
{"success": false, "error": "File not found: missing.txt"}
{"success": false, "error": "Permission denied: /etc/shadow"}
{"success": false, "error": "Invalid glob pattern: [unclosed"}
{"success": false, "error": "Command execution failed: popen returned NULL"}
```

## Design Rationale

1. **Consistent envelope**: Every result has `success` boolean - easy to check
2. **Tool-specific data**: Each tool can include relevant fields in `data`
3. **Human-readable errors**: Error messages can be shown directly to user
4. **Bash distinction**: Exit codes are valid output, not tool errors
