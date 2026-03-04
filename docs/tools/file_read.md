# file_read

## NAME

file_read - read contents of a file

## SYNOPSIS

```json
{"name": "file_read", "arguments": {"file_path": "..."}}
```

## DESCRIPTION

`file_read` reads a file and returns its contents as a string. It supports pagination via `offset` and `limit` to read large files in chunks without loading everything into context at once.

Paths may be absolute or relative to the ikigai working directory.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `file_path` | string | yes | Absolute or relative path to the file to read. |
| `offset` | integer | no | Line number to start reading from (1-based). Omit to start from the beginning. |
| `limit` | integer | no | Maximum number of lines to return. Omit to read to end of file. |

## RETURNS

The file contents as a string (with line numbers prepended in some implementations). When `offset` and `limit` are used, only the requested line range is returned.

## EXAMPLES

Read an entire file:

```json
{
  "name": "file_read",
  "arguments": {
    "file_path": "docs/README.md"
  }
}
```

Read lines 50–100 of a large file:

```json
{
  "name": "file_read",
  "arguments": {
    "file_path": "apps/ikigai/repl.c",
    "offset": 50,
    "limit": 50
  }
}
```

Read a file starting at a known section:

```json
{
  "name": "file_read",
  "arguments": {
    "file_path": "project/decisions/001-provider-abstraction.md",
    "offset": 30
  }
}
```

## NOTES

Always read a file before editing it with `file_edit`. Editing without reading first is an error.

For large files, use `offset` and `limit` to read incrementally. Combine with `grep` to find the relevant line range first.

## SEE ALSO

[file_write](file_write.md), [file_edit](file_edit.md), [grep](grep.md), [glob](glob.md), [Tools](../tools.md)
