# Agent Reads Message

## Description

An agent uses the `mail` tool with `action: read` to retrieve the full content of a specific message. The message is automatically marked as read.

## Transcript

```text
I have 2 unread messages. Let me read message #5 from agent 1/.

[Tool use: mail]
  action: read
  id: 5

[Tool result]:
{
  "id": 5,
  "from": "1/",
  "timestamp": "2024-01-15T10:30:00Z",
  "body": "Found 3 OAuth patterns worth considering:\n\n1) Silent refresh - refresh token in background before expiry\n2) Retry with refresh - catch 401, refresh, retry original request\n3) Token rotation - each refresh returns new refresh token\n\nRecommend pattern #2 for simplicity."
}

Based on agent 1/'s research, I'll implement the retry-with-refresh pattern...
```

## Walkthrough

1. Agent calls `mail` tool with `action: read` and `id: 5`

2. Tool handler looks up message by ID in agent's inbox

3. If message not found, returns error:
   ```json
   {"error": "Message #5 not found"}
   ```

4. If found, handler builds response JSON:
   - `id`: message ID
   - `from`: sender agent ID
   - `timestamp`: ISO 8601 timestamp
   - `body`: full message content (no truncation)

5. Handler marks message as read (`msg->read = true`)

6. Handler decrements agent's `unread_count`

7. Handler updates database (set `read = 1`)

8. Tool result returned to agent

9. Agent can now use the information in the message

10. Reading message clears `mail_notification_pending` flag

---

## Reference

### Tool Call

```json
{
  "name": "mail",
  "arguments": "{\"action\": \"read\", \"id\": 5}"
}
```

### Tool Result (Success)

```json
{
  "id": 5,
  "from": "1/",
  "timestamp": "2024-01-15T10:30:00Z",
  "body": "Found 3 OAuth patterns worth considering:\n\n1) Silent refresh..."
}
```

### Tool Result (Not Found)

```json
{
  "error": "Message #5 not found"
}
```
