# glob

## NAME

glob - find files matching a glob pattern

## SYNOPSIS

```json
{"name": "glob", "arguments": {"pattern": "..."}}
```

## DESCRIPTION

`glob` searches for files whose paths match a glob pattern and returns a list of matching paths. It supports standard glob syntax including `*` (any characters except `/`), `**` (any path component including `/`), and `?` (any single character).

Use `glob` to discover files by name or extension — for example, finding all C source files, all test files, or all Markdown documents. To search file *contents*, use `grep`.

By default, the search is rooted at the ikigai working directory. Use `path` to restrict the search to a subdirectory.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `pattern` | string | yes | Glob pattern to match against file paths. Examples: `*.c`, `src/**/*.h`, `tests/*_test.c`. |
| `path` | string | no | Directory to search in. Default: current working directory. |

## RETURNS

A list of matching file paths, sorted by modification time (most recently modified first). Returns an empty list if no files match.

## EXAMPLES

Find all C source files in the project:

```json
{
  "name": "glob",
  "arguments": {
    "pattern": "**/*.c"
  }
}
```

Find all unit test files:

```json
{
  "name": "glob",
  "arguments": {
    "pattern": "*_test.c",
    "path": "tests/unit"
  }
}
```

Find all Markdown documentation:

```json
{
  "name": "glob",
  "arguments": {
    "pattern": "**/*.md",
    "path": "docs"
  }
}
```

Find all header files in a specific module:

```json
{
  "name": "glob",
  "arguments": {
    "pattern": "*.h",
    "path": "apps/ikigai"
  }
}
```

## NOTES

`**` matches across directory boundaries — `**/*.c` finds all `.c` files at any depth. `*.c` (without `**`) matches only in the root of the search path.

Results are sorted by modification time, not alphabetically. The most recently changed files appear first.

For content-based searching, use `grep`. For reading a specific file, use `file_read`.

## SEE ALSO

[grep](grep.md), [file_read](file_read.md), [Tools](../tools.md)
