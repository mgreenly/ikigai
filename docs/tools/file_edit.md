# file_edit

## NAME

file_edit - edit a file by replacing an exact text match

## SYNOPSIS

```json
{"name": "file_edit", "arguments": {"file_path": "...", "old_string": "...", "new_string": "..."}}
```

## DESCRIPTION

`file_edit` replaces an exact substring in a file with new text. The match must be unique — if `old_string` appears more than once, the edit is rejected unless `replace_all` is set to `true`.

You must read the file with `file_read` before calling `file_edit`. This ensures the text you intend to replace actually exists and is accurate.

Paths may be absolute or relative to the ikigai working directory.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `file_path` | string | yes | Absolute or relative path to the file to edit. |
| `old_string` | string | yes | Exact text to find and replace. Must match verbatim including whitespace and indentation. |
| `new_string` | string | yes | Text to substitute in place of `old_string`. |
| `replace_all` | boolean | no | If `true`, replaces every occurrence of `old_string`. Default: `false`. Fails if `old_string` is not unique and this is `false`. |

## RETURNS

On success, a confirmation message. On failure, an error explaining why the edit could not be applied (text not found, not unique, permission denied).

## EXAMPLES

Fix a typo in a source file:

```json
{
  "name": "file_edit",
  "arguments": {
    "file_path": "apps/ikigai/repl.c",
    "old_string": "    return ERR(\"conection failed\");\n",
    "new_string": "    return ERR(\"connection failed\");\n"
  }
}
```

Rename a variable throughout a file:

```json
{
  "name": "file_edit",
  "arguments": {
    "file_path": "apps/ikigai/agent.c",
    "old_string": "ctx->timeout",
    "new_string": "ctx->deadline",
    "replace_all": true
  }
}
```

Replace a function signature:

```json
{
  "name": "file_edit",
  "arguments": {
    "file_path": "apps/ikigai/tool_registry.c",
    "old_string": "Result tool_register(Registry *r, const char *name) {",
    "new_string": "Result tool_register(Registry *r, const char *name, int flags) {"
  }
}
```

## NOTES

The match is byte-exact. Include enough surrounding context in `old_string` to make it unique if the short form would match multiple locations.

Read the file first. If the file has been modified since you last read it, your `old_string` may no longer match.

Use `replace_all: true` when renaming identifiers consistently across a file. For single targeted changes, leave `replace_all` at its default to guard against accidental mass replacement.

## SEE ALSO

[file_read](file_read.md), [file_write](file_write.md), [Tools](../tools.md)
