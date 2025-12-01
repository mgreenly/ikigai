# Single File Read

## Description

User asks to read a specific file. The model calls file_read, receives the contents, and displays them to the user.

## Transcript

```text
> Show me the contents of README.md

Here's the contents of README.md:

# My Project

A simple example project.
```

## Walkthrough

1. User types "Show me the contents of README.md" and presses Enter

2. Client builds chat completion request with user message and tools array (see [Request A](#request-a))

3. Request sent to OpenAI API

4. Model responds with a tool call to file_read (see [Response A](#response-a))

5. Tool call displayed in scrollback

6. Client executes file_read with path `README.md`

7. Tool returns file contents (see [Tool Result A](#tool-result-a))

8. Tool result displayed in scrollback

9. Client builds follow-up request with tool result (see [Request B](#request-b))

10. Model presents the file contents (see [Response B](#response-b))

11. Response streams to scrollback

12. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Show me the contents of README.md"}
  ],
  "tools": ["...standard tools array..."],
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
        "id": "call_def456",
        "type": "function",
        "function": {
          "name": "file_read",
          "arguments": "{\"path\": \"README.md\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"success": true, "data": {"output": "# My Project\n\nA simple example project."}}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Show me the contents of README.md"},
    {"role": "assistant", "tool_calls": [{"id": "call_def456", "type": "function", "function": {"name": "file_read", "arguments": "{\"path\": \"README.md\"}"}}]},
    {"role": "tool", "tool_call_id": "call_def456", "content": "{\"success\": true, \"data\": {\"output\": \"# My Project\\n\\nA simple example project.\"}}"}
  ],
  "tools": ["...standard tools array..."],
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
      "content": "Here's the contents of README.md:\n\n# My Project\n\nA simple example project."
    },
    "finish_reason": "stop"
  }]
}
```
