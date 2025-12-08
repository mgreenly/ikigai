# File Not Found Error

## Description

User asks to read a file that doesn't exist. The tool returns an error, which is passed to the model. The model explains the problem to the user.

## Transcript

```text
> Show me missing.txt

I couldn't find that file. `missing.txt` doesn't exist in the current directory. Would you like me to search for it elsewhere?
```

## Walkthrough

1. User types "Show me missing.txt" and presses Enter

2. Client builds chat completion request (see [Request A](#request-a))

3. Model responds with file_read tool call (see [Response A](#response-a))

4. Tool call displayed in scrollback

5. Client attempts to execute file_read with path `missing.txt`

6. Tool returns error (see [Tool Result A](#tool-result-a))

7. Error result displayed in scrollback

8. Client builds follow-up request with error result (see [Request B](#request-b))

9. Model explains the error and offers alternatives (see [Response B](#response-b))

10. Response streams to scrollback

11. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Show me missing.txt"}
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
        "id": "call_read1",
        "type": "function",
        "function": {
          "name": "file_read",
          "arguments": "{\"path\": \"missing.txt\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"success": false, "error": "File not found: missing.txt"}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Show me missing.txt"},
    {"role": "assistant", "tool_calls": [{"id": "call_read1", "type": "function", "function": {"name": "file_read", "arguments": "{\"path\": \"missing.txt\"}"}}]},
    {"role": "tool", "tool_call_id": "call_read1", "content": "{\"success\": false, \"error\": \"File not found: missing.txt\"}"}
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
      "content": "I couldn't find that file. `missing.txt` doesn't exist in the current directory. Would you like me to search for it elsewhere?"
    },
    "finish_reason": "stop"
  }]
}
```
