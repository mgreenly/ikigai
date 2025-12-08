# Tool Choice: None

## Description

Demonstrates `tool_choice: "none"` behavior. The model cannot call tools and must respond with text only.

## Transcript

```text
> Find all C files in src/

To find all C files in src/, you can use a command like `find src/ -name "*.c"` or check the directory listing. I don't have access to your filesystem to search directly.
```

## Walkthrough

1. User types "Find all C files in src/" and presses Enter

2. Client builds chat completion request with `tool_choice: "none"` (see [Request A](#request-a))

3. Model responds with text only, no tool calls (see [Response A](#response-a))

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
  "tool_choice": "none",
  "stream": true
}
```

### Response A

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "content": "To find all C files in src/, you can use a command like `find src/ -name \"*.c\"` or check the directory listing. I don't have access to your filesystem to search directly."
    },
    "finish_reason": "stop"
  }]
}
```
