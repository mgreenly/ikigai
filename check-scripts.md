# Check Scripts Specification

All `check-*` scripts must follow this output contract.

## Output Format

Scripts must output **only** JSON to stdout in one of two forms:

### Success
```json
{"ok":true}
```

### Failure
```json
{"ok":false,"items":["file1","file2",...]}
```

Where `items` is an array of strings providing a list of files with issues.

## Requirements

1. **JSON only** - No other output to stdout
2. **Machine-readable** - Must be valid JSON
3. **File-focused** - Items should identify specific files
4. **Silent by default** - Progress/logging only to stderr or when `--verbose` flag is used
5. **Exit codes** - Return 0 for success (`ok:true`), non-zero for failure (`ok:false`)

## Optional Flags

### `--file=<path>`
When provided, filter results to only the specified file and include detailed information useful for fixing issues.

**Behavior:**
- If file doesn't exist: `{"ok":false,"items":["<path>: file not found"]}`
- If file exists with no issues: `{"ok":true}`
- If file exists with issues: `{"ok":false,"items":[...]}` where items contain detailed, actionable information about each issue in that file

### `--verbose`
Enable progress output and spinners to stderr (not stdout).

## Examples

### check-compile (default mode)
```json
{"ok":false,"items":[
  "src/foo.c:42:5: error: implicit declaration of function 'bar'",
  "src/baz.c:15:10: error: unknown type name 'Widget'"
]}
```

### check-compile (with --file=src/foo.c)
```json
{"ok":false,"items":[
  "src/foo.c:42:5: error: implicit declaration of function 'bar'",
  "src/foo.c:43:12: note: did you mean 'baz'?",
  "src/foo.c:100:8: error: conflicting types for 'process'"
]}
```

### check-coverage (default mode)
```json
{"ok":false,"items":[
  "src/array.c lines:85.2% funcs:90.9% branches:87.5%",
  "src/string.c lines:88.0% funcs:85.0% branches:90.0%"
]}
```

### check-coverage (with --file=src/array.c)
```json
{"ok":false,"items":[
  "lines: 85.2% (missing: 42, 45-48, 103)",
  "funcs: 90.9% (missing: array_resize)",
  "branches: 87.5% (line 55 branch 1, line 78 branch 0)"
]}
```

### check-filesize
```json
{"ok":false,"items":[
  "src/huge.c: 18432 bytes (exceeds 16384 limit)"
]}
```
