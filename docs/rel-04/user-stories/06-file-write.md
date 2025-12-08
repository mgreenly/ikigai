# File Write

## Description

User asks to create a new file with specific content. The model uses file_write to create the file and confirms success.

## Transcript

```text
> Create a file called notes.txt with "Remember to refactor"

Done! I've created notes.txt with your message.
```

## Walkthrough

1. User types "Create a file called notes.txt with \"Remember to refactor\"" and presses Enter

2. Client builds chat completion request (see [Request A](#request-a))

3. Model responds with file_write tool call (see [Response A](#response-a))

4. Tool call displayed in scrollback

5. Client executes file_write with path `notes.txt` and content `Remember to refactor`

6. Tool returns success confirmation (see [Tool Result A](#tool-result-a))

7. Tool result displayed in scrollback

8. Client builds follow-up request with tool result (see [Request B](#request-b))

9. Model confirms the file was created (see [Response B](#response-b))

10. Response streams to scrollback

11. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Create a file called notes.txt with \"Remember to refactor\""}
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
        "id": "call_write1",
        "type": "function",
        "function": {
          "name": "file_write",
          "arguments": "{\"path\": \"notes.txt\", \"content\": \"Remember to refactor\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"success": true, "data": {"output": "Wrote 20 bytes to notes.txt", "bytes": 20}}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Create a file called notes.txt with \"Remember to refactor\""},
    {"role": "assistant", "tool_calls": [{"id": "call_write1", "type": "function", "function": {"name": "file_write", "arguments": "{\"path\": \"notes.txt\", \"content\": \"Remember to refactor\"}"}}]},
    {"role": "tool", "tool_call_id": "call_write1", "content": "{\"success\": true, \"data\": {\"output\": \"Wrote 20 bytes to notes.txt\", \"bytes\": 20}}"}
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
      "content": "Done! I've created notes.txt with your message."
    },
    "finish_reason": "stop"
  }]
}
```
