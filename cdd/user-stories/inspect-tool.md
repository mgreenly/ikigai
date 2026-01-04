# Inspect Tool

User inspects a tool's schema with `/tool NAME`.

## Description

User runs `/tool cat` to see the full schema for a specific tool. Shows parameters, types, descriptions - everything the LLM sees.

## Transcript

```text
> /tool cat

cat - Read file contents

Parameters:
  path (string, required)
    Path to file
```

## Walkthrough

1. User types `/tool cat` and presses Enter
2. ikigai parses command, extracts tool name "cat"
3. ikigai looks up "cat" in tool registry
4. If found: format and display schema
5. If not found: show error message

## Error Case

```text
> /tool nonexistent

Tool 'nonexistent' not found. Run /tool to see available tools.
```

## Notes

- No LLM call, no tokens used
- Shows the exact schema the LLM receives
- Useful for debugging custom tools
- Parameters show type, required flag, description
