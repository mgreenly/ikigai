# grep

## NAME

grep - search for a pattern in files using regular expressions

## SYNOPSIS

```json
{"name": "grep", "arguments": {"pattern": "..."}}
```

## DESCRIPTION

`grep` searches file contents for lines matching a regular expression and returns the matching lines with file paths and line numbers. It uses POSIX extended regular expression syntax.

Use `grep` to find where something is defined or used — for example, locating a function definition, finding all callers of an API, or searching for a string across the codebase. To find files by name, use `glob`.

By default, the search covers the entire ikigai working directory. Use `path` to restrict to a subdirectory and `glob` (the parameter, not the tool) to filter by file extension.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `pattern` | string | yes | Regular expression to search for (POSIX extended syntax). |
| `glob` | string | no | Glob pattern to filter which files are searched (e.g., `*.c`, `*.md`). |
| `path` | string | no | Directory to search in. Default: current working directory. |

## RETURNS

A list of matches, each with a file path, line number, and the matching line's content. Returns an empty result if no matches are found.

## EXAMPLES

Find all calls to a function:

```json
{
  "name": "grep",
  "arguments": {
    "pattern": "tool_register\\(",
    "glob": "*.c"
  }
}
```

Search for a string in documentation:

```json
{
  "name": "grep",
  "arguments": {
    "pattern": "sliding context",
    "path": "docs",
    "glob": "*.md"
  }
}
```

Find all `TODO` comments in source:

```json
{
  "name": "grep",
  "arguments": {
    "pattern": "TODO",
    "glob": "*.c",
    "path": "apps/ikigai"
  }
}
```

Locate a struct definition:

```json
{
  "name": "grep",
  "arguments": {
    "pattern": "^typedef struct Agent",
    "glob": "*.h"
  }
}
```

## NOTES

The `glob` parameter here is a file filter, not a call to the `glob` tool. It restricts which files are searched based on filename patterns.

Regular expressions follow POSIX ERE syntax. Special characters like `(`, `)`, `[`, `]`, `.`, `*`, `+`, `?`, `{`, `}`, `^`, `$`, `|`, and `\` have special meaning and must be escaped with `\` if you want them treated literally.

For finding files by name rather than content, use the `glob` tool.

## SEE ALSO

[glob](glob.md), [file_read](file_read.md), [Tools](../tools.md)
