# Tool Choice: Auto

## Description

Demonstrates `tool_choice: "auto"` behavior. The model decides whether to call a tool or respond with text. This is the default behavior.

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

2. Client builds chat completion request with `tool_choice: "auto"` (see [Request A](#request-a))

3. Model decides to call glob tool (see [Response A](#response-a))

4. Client executes glob, returns results (see [Tool Result A](#tool-result-a))

5. Client builds follow-up request (see [Request B](#request-b))

6. Model summarizes results (see [Response B](#response-b))

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Find all C files in src/"}
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
{"output": "src/main.c\nsrc/config.c\nsrc/repl.c", "count": 3}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Find all C files in src/"},
    {"role": "assistant", "tool_calls": [{"id": "call_abc123", "type": "function", "function": {"name": "glob", "arguments": "{\"pattern\": \"*.c\", \"path\": \"src/\"}"}}]},
    {"role": "tool", "tool_call_id": "call_abc123", "content": "{\"output\": \"src/main.c\\nsrc/config.c\\nsrc/repl.c\", \"count\": 3}"}
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
      "content": "I found the following C files in src/:\n\n- src/main.c\n- src/config.c\n- src/repl.c"
    },
    "finish_reason": "stop"
  }]
}
```
