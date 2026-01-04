# List Tools

User lists available tools with `/tool` command.

## Description

User runs `/tool` to see all discovered tools. This is a local command - no LLM call, no tokens used.

## Transcript

```text
> /tool

Available tools:
  bash          Execute a shell command and return output
  file_read     Read contents of a file
  file_write    Write content to a file (creates or overwrites)
  file_edit     Edit a file by replacing exact text matches. You must read the file before editing.
  glob          Find files matching a glob pattern
  grep          Search for pattern in files using regular expressions
```

## Walkthrough

1. User types `/tool` and presses Enter
2. ikigai recognizes slash command, does not send to LLM
3. ikigai iterates tool registry
4. For each tool: print name and description (from schema)
5. Output appears immediately (no network, no streaming)

## Notes

- Command works even if LLM provider is down
- Shows tools from both system and user directories
- User tools that override system tools show user version
- Tools that failed schema discovery are not listed
