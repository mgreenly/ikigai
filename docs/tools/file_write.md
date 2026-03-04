# file_write

## NAME

file_write - write content to a file, creating or overwriting it

## SYNOPSIS

```json
{"name": "file_write", "arguments": {"file_path": "...", "content": "..."}}
```

## DESCRIPTION

`file_write` writes a string to a file. If the file does not exist, it is created (including any missing parent directories). If the file already exists, it is overwritten in full.

Use this tool to create new files or completely replace an existing file's content. For making targeted edits to an existing file, prefer `file_edit` — it is safer and easier to review.

Paths may be absolute or relative to the ikigai working directory.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `file_path` | string | yes | Absolute or relative path to the file to write. |
| `content` | string | yes | Content to write. Overwrites any existing content. |

## RETURNS

On success, a confirmation message. On failure, an error describing what went wrong (e.g., permission denied, invalid path).

## EXAMPLES

Create a new file:

```json
{
  "name": "file_write",
  "arguments": {
    "file_path": "docs/tools/new-tool.md",
    "content": "# new-tool\n\nDocumentation goes here.\n"
  }
}
```

Overwrite a configuration file:

```json
{
  "name": "file_write",
  "arguments": {
    "file_path": ".env.local",
    "content": "IKIGAI_LOG_LEVEL=debug\nIKIGAI_DB_HOST=localhost\n"
  }
}
```

## NOTES

This tool overwrites the entire file. There is no append mode. To modify part of a file, use `file_edit` (exact replacement) or read the file first, modify the content in the prompt, then write it back.

Read the existing file before overwriting if you need to preserve any of its content.

## SEE ALSO

[file_read](file_read.md), [file_edit](file_edit.md), [Tools](../tools.md)
