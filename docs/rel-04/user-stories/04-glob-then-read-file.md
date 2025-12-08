# Glob Then Read File

## Description

User asks to find and read a config file. The model first uses glob to locate the file, then uses file_read to get its contents. Demonstrates sequential tool calls across multiple turns.

## Transcript

```text
> Find the config file and show me what's in it

I found config.json. Here are its contents:

{
  "debug": true,
  "port": 8080
}
```

## Walkthrough

1. User types "Find the config file and show me what's in it" and presses Enter

2. Client builds chat completion request (see [Request A](#request-a))

3. Model responds with glob tool call (see [Response A](#response-a))

4. Tool call displayed in scrollback

5. Client executes glob with pattern `config.*`

6. Tool returns matching file (see [Tool Result A](#tool-result-a))

7. Tool result displayed in scrollback

8. Client builds follow-up request with tool result (see [Request B](#request-b))

9. Model responds with file_read tool call (see [Response B](#response-b))

10. Tool call displayed in scrollback

11. Client executes file_read with path `config.json`

12. Tool returns file contents (see [Tool Result B](#tool-result-b))

13. Tool result displayed in scrollback

14. Client builds follow-up request with second tool result (see [Request C](#request-c))

15. Model presents the file contents (see [Response C](#response-c))

16. Response streams to scrollback

17. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Find the config file and show me what's in it"}
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
        "id": "call_glob1",
        "type": "function",
        "function": {
          "name": "glob",
          "arguments": "{\"pattern\": \"config.*\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"success": true, "data": {"output": "config.json", "count": 1}}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Find the config file and show me what's in it"},
    {"role": "assistant", "tool_calls": [{"id": "call_glob1", "type": "function", "function": {"name": "glob", "arguments": "{\"pattern\": \"config.*\"}"}}]},
    {"role": "tool", "tool_call_id": "call_glob1", "content": "{\"success\": true, \"data\": {\"output\": \"config.json\", \"count\": 1}}"}
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
      "tool_calls": [{
        "index": 0,
        "id": "call_read1",
        "type": "function",
        "function": {
          "name": "file_read",
          "arguments": "{\"path\": \"config.json\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result B

```json
{"success": true, "data": {"output": "{\n  \"debug\": true,\n  \"port\": 8080\n}"}}
```

### Request C

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Find the config file and show me what's in it"},
    {"role": "assistant", "tool_calls": [{"id": "call_glob1", "type": "function", "function": {"name": "glob", "arguments": "{\"pattern\": \"config.*\"}"}}]},
    {"role": "tool", "tool_call_id": "call_glob1", "content": "{\"output\": \"config.json\", \"count\": 1}"},
    {"role": "assistant", "tool_calls": [{"id": "call_read1", "type": "function", "function": {"name": "file_read", "arguments": "{\"path\": \"config.json\"}"}}]},
    {"role": "tool", "tool_call_id": "call_read1", "content": "{\"success\": true, \"data\": {\"output\": \"{\\n  \\\"debug\\\": true,\\n  \\\"port\\\": 8080\\n}\"}}"}
  ],
  "tools": ["...standard tools array..."],
  "tool_choice": "auto",
  "stream": true
}
```

### Response C

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "content": "I found config.json. Here are its contents:\n\n{\n  \"debug\": true,\n  \"port\": 8080\n}"
    },
    "finish_reason": "stop"
  }]
}
```
