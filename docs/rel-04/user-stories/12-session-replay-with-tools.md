# Session Replay With Tools

## Description

User restarts ikigai and the previous session is restored from the database. The session included tool calls which are replayed into the conversation history, allowing the model to maintain context.

## Transcript

```text
[Session restored: 3 messages]

> What file did you read earlier?

Earlier I read config.json, which contained your debug and port settings.
```

## Walkthrough

1. User launches ikigai

2. Client detects existing active session in database

3. Client loads session events from database

4. Replay algorithm reconstructs conversation state:
   - User message: "Show me config.json"
   - Assistant message with tool_call to file_read
   - Tool result with file contents
   - Assistant message summarizing contents

5. "[Session restored: 3 messages]" displayed in scrollback

6. User types "What file did you read earlier?" and presses Enter

7. Client builds request with full conversation history (see [Request A](#request-a))

8. Model responds using context from replayed history (see [Response A](#response-a))

9. Response streams to scrollback

10. New messages persisted to database

---

## Reference

### Database Events (Replayed)

```
Event 1: {type: "user_message", content: "Show me config.json"}
Event 2: {type: "tool_call", tool: "file_read", args: {path: "config.json"}, call_id: "call_xyz"}
Event 3: {type: "tool_result", call_id: "call_xyz", content: "{\"success\": true, \"data\": {\"output\": \"{\\\"debug\\\": true, \\\"port\\\": 8080}\"}}"}
Event 4: {type: "assistant_message", content: "Here's config.json with your debug and port settings."}
```

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Show me config.json"},
    {"role": "assistant", "tool_calls": [{"id": "call_xyz", "type": "function", "function": {"name": "file_read", "arguments": "{\"path\": \"config.json\"}"}}]},
    {"role": "tool", "tool_call_id": "call_xyz", "content": "{\"success\": true, \"data\": {\"output\": \"{\\\"debug\\\": true, \\\"port\\\": 8080}\"}}"},
    {"role": "assistant", "content": "Here's config.json with your debug and port settings."},
    {"role": "user", "content": "What file did you read earlier?"}
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
      "content": "Earlier I read config.json, which contained your debug and port settings."
    },
    "finish_reason": "stop"
  }]
}
```
