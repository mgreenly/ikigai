# Add Custom Tool

User adds a custom tool to their tools directory and uses it.

## Description

User creates a shell script that follows the tool protocol, places it in `~/.ikigai/tools/`, runs `/refresh`, and the tool becomes available to the LLM.

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

> /refresh

Tools refreshed. 7 tools available.

> /tool

Available tools:
  bash          Execute a shell command and return output
  cat           Read file contents
  file_read     Read contents of a file
  file_write    Write content to a file (creates or overwrites)
  file_edit     Edit a file by replacing exact text matches. You must read the file before editing.
  glob          Find files matching a glob pattern
  grep          Search for pattern in files using regular expressions

> read /etc/hostname using the cat tool

I'll read that file for you.

The hostname is: dev-workstation
```

## Walkthrough

1. User runs `/tool` to see current tools (6 system tools, no `cat`)
2. User creates `~/.ikigai/tools/cat` (the test tool script)
3. User runs `/refresh` to trigger tool discovery
4. ikigai scans `~/.ikigai/tools/`, finds `cat`, calls `cat --schema`
5. Schema is valid JSON, tool added to registry
6. User runs `/tool` to verify `cat` is now listed (7 tools total)
7. User asks LLM to use the tool
8. LLM calls tool (see [Tool Call](#tool-call))
9. ikigai executes: `echo '{"path": "/etc/hostname"}' | ~/.ikigai/tools/cat`
10. Tool returns JSON (see [Tool Response](#tool-response))
11. ikigai wraps response (see [Wrapped Response](#wrapped-response))
12. LLM receives result, formats answer for user

## Reference

### Tool Call

LLM's tool call in the response:

```json
{
  "tool_calls": [
    {
      "id": "call_abc123",
      "type": "function",
      "function": {
        "name": "cat",
        "arguments": "{\"path\": \"/etc/hostname\"}"
      }
    }
  ]
}
```

### Tool Response

Raw output from the tool:

```json
{
  "content": "dev-workstation\n"
}
```

### Wrapped Response

What ikigai sends to LLM:

```json
{
  "tool_success": true,
  "result": {
    "content": "dev-workstation\n"
  }
}
```

## Error Cases

### Missing Parameter

```text
> read a file with cat but don't specify which one

I need to know which file to read. What file path would you like me to read?
```

If LLM calls tool without path:

```json
{"error": "missing required parameter: path"}
```

Wrapped:

```json
{
  "tool_success": true,
  "result": {
    "error": "missing required parameter: path"
  }
}
```

LLM sees the error and asks user for clarification.

### File Not Found

```text
> read /nonexistent/file using cat

That file doesn't exist. Would you like me to check a different path?
```

Tool response:

```json
{"error": "file not found: /nonexistent/file"}
```

LLM handles gracefully.
