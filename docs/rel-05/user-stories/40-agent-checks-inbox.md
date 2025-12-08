# Agent Checks Inbox

## Description

An agent uses the `mail` tool with `action: inbox` to list messages in their inbox. This is the programmatic equivalent of the `/mail` command.

## Transcript

```text
[Notification: You have 2 unread messages in your inbox]

I'll check my inbox to see what messages are waiting.

[Tool use: mail]
  action: inbox

[Tool result]:
{
  "messages": [
    {"id": 5, "from": "1/", "unread": true, "preview": "Found 3 OAuth patterns..."},
    {"id": 4, "from": "2/", "unread": true, "preview": "Build complete, all..."},
    {"id": 3, "from": "1/", "unread": false, "preview": "Starting research..."}
  ],
  "unread_count": 2
}

I have 2 unread messages. Let me read the first one from agent 1/.
```

## Walkthrough

1. Agent decides to check inbox (often after notification)

2. Agent calls `mail` tool with `action: inbox`

3. Tool handler retrieves agent's inbox

4. Handler builds response JSON:
   - `messages`: array of message summaries
   - Each message: `id`, `from`, `unread`, `preview`
   - `preview`: first ~50 chars of body
   - `unread_count`: total unread messages

5. Messages ordered: unread first, then by timestamp descending

6. Tool result returned to agent

7. Agent can reason about messages and decide which to read

8. Checking inbox clears `mail_notification_pending` flag

9. Agent typically follows up with `action: read` for specific messages

---

## Reference

### Tool Definition

```json
{
  "type": "function",
  "function": {
    "name": "mail",
    "description": "Send and receive messages to/from other agents",
    "parameters": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "enum": ["inbox", "read", "send"],
          "description": "Operation to perform"
        },
        "to": {
          "type": "string",
          "description": "Recipient agent ID (required for send)"
        },
        "body": {
          "type": "string",
          "description": "Message body (required for send)"
        },
        "id": {
          "type": "integer",
          "description": "Message ID (required for read)"
        }
      },
      "required": ["action"]
    }
  }
}
```

### Tool Call

```json
{
  "name": "mail",
  "arguments": "{\"action\": \"inbox\"}"
}
```

### Tool Result

```json
{
  "messages": [
    {"id": 5, "from": "1/", "unread": true, "preview": "Found 3 OAuth patterns..."},
    {"id": 4, "from": "2/", "unread": true, "preview": "Build complete, all..."},
    {"id": 3, "from": "1/", "unread": false, "preview": "Starting research..."}
  ],
  "unread_count": 2
}
```
