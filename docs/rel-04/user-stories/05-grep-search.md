# Grep Search

## Description

User asks to search for a pattern in the codebase. The model uses grep to find matches and reports the results.

## Transcript

```text
> Search for TODO comments in the C files

I found TODO comments in the following locations:

- src/main.c:42: // TODO: add error handling
- src/repl.c:15: // TODO: implement history
```

## Walkthrough

1. User types "Search for TODO comments in the C files" and presses Enter

2. Client builds chat completion request (see [Request A](#request-a))

3. Model responds with grep tool call (see [Response A](#response-a))

4. Tool call displayed in scrollback

5. Client executes grep with pattern `TODO` and glob `*.c`

6. Tool returns matching lines with file locations (see [Tool Result A](#tool-result-a))

7. Tool result displayed in scrollback

8. Client builds follow-up request with tool result (see [Request B](#request-b))

9. Model summarizes the search results (see [Response B](#response-b))

10. Response streams to scrollback

11. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Search for TODO comments in the C files"}
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
        "id": "call_grep1",
        "type": "function",
        "function": {
          "name": "grep",
          "arguments": "{\"pattern\": \"TODO\", \"glob\": \"*.c\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"success": true, "data": {"output": "src/main.c:42: // TODO: add error handling\nsrc/repl.c:15: // TODO: implement history", "count": 2}}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Search for TODO comments in the C files"},
    {"role": "assistant", "tool_calls": [{"id": "call_grep1", "type": "function", "function": {"name": "grep", "arguments": "{\"pattern\": \"TODO\", \"glob\": \"*.c\"}"}}]},
    {"role": "tool", "tool_call_id": "call_grep1", "content": "{\"success\": true, \"data\": {\"output\": \"src/main.c:42: // TODO: add error handling\\nsrc/repl.c:15: // TODO: implement history\", \"count\": 2}}"}
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
      "content": "I found TODO comments in the following locations:\n\n- src/main.c:42: // TODO: add error handling\n- src/repl.c:15: // TODO: implement history"
    },
    "finish_reason": "stop"
  }]
}
```
