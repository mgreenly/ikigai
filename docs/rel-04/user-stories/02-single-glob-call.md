# Single Glob Call

## Description

User asks to find C source files. The model calls the glob tool once, receives results, and summarizes them to the user.

## Transcript

```text
> Find all C files in src/

I found the following C files in src/:

- src/main.c
- src/config.c
- src/repl.c
```

## Walkthrough

1. User types "Find all C files in src/" and presses Enter

2. Client builds chat completion request with user message and tools array (see [Request A](#request-a))

3. Request sent to OpenAI API

4. Model responds with a tool call to glob (see [Response A](#response-a))

5. Tool call displayed in scrollback

6. Client executes glob with pattern `*.c` in path `src/`

7. Tool returns list of matching files (see [Tool Result A](#tool-result-a))

8. Tool result displayed in scrollback

9. Client builds follow-up request with tool result (see [Request B](#request-b))

10. Model summarizes results for user (see [Response B](#response-b))

11. Response streams to scrollback

12. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Find all C files in src/"}
  ],
  "tools": [
    {"type": "function", "function": {"name": "glob", "description": "Find files matching a glob pattern", "parameters": {"type": "object", "properties": {"pattern": {"type": "string"}, "path": {"type": "string"}}, "required": ["pattern"]}}},
    {"type": "function", "function": {"name": "file_read", "description": "Read contents of a file", "parameters": {"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}}},
    {"type": "function", "function": {"name": "grep", "description": "Search file contents for a pattern", "parameters": {"type": "object", "properties": {"pattern": {"type": "string"}, "path": {"type": "string"}, "glob": {"type": "string"}}, "required": ["pattern"]}}},
    {"type": "function", "function": {"name": "file_write", "description": "Write content to a file", "parameters": {"type": "object", "properties": {"path": {"type": "string"}, "content": {"type": "string"}}, "required": ["path", "content"]}}},
    {"type": "function", "function": {"name": "bash", "description": "Execute a shell command", "parameters": {"type": "object", "properties": {"command": {"type": "string"}}, "required": ["command"]}}}
  ],
  "tool_choice": "auto",
  "stream": true
}
```

### Response A

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "tool_calls": [{
        "index": 0,
        "id": "call_abc123",
        "type": "function",
        "function": {
          "name": "glob",
          "arguments": "{\"pattern\": \"*.c\", \"path\": \"src/\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"success": true, "data": {"output": "src/main.c\nsrc/config.c\nsrc/repl.c", "count": 3}}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Find all C files in src/"},
    {"role": "assistant", "tool_calls": [{"id": "call_abc123", "type": "function", "function": {"name": "glob", "arguments": "{\"pattern\": \"*.c\", \"path\": \"src/\"}"}}]},
    {"role": "tool", "tool_call_id": "call_abc123", "content": "{\"success\": true, \"data\": {\"output\": \"src/main.c\\nsrc/config.c\\nsrc/repl.c\", \"count\": 3}}"}
  ],
  "tools": ["...same as Request A..."],
  "tool_choice": "auto",
  "stream": true
}
```

### Response B

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "content": "I found the following C files in src/:\n\n- src/main.c\n- src/config.c\n- src/repl.c"
    },
    "finish_reason": "stop"
  }]
}
```
