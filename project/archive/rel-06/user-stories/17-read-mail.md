# Read Mail

## Description

User runs `/read-mail <id>` to read a specific message and mark it as read.

## Transcript

```text
───────────────────────── agent [xyz789...] ─────────────────────────
> /read-mail 1

From: abc123...
Date: 2 minutes ago
─────────────────────────

Please research OAuth 2.0 patterns for our authentication
system. Focus on PKCE flow for single-page applications.

> _
```

## Walkthrough

1. User types `/read-mail 1`
2. Handler looks up message by ID in current agent's inbox
3. Handler validates message belongs to current agent
4. Handler displays full message:
   - Sender UUID
   - Timestamp
   - Full body (no truncation)
5. Handler marks message as read (`read = true`)
6. Subsequent `/check-mail` shows no `*` marker

## Reference

```json
{
  "command": "/read-mail <id>",
  "arguments": {
    "id": "message ID from /check-mail list"
  },
  "actions": {
    "display": "full message content",
    "mark_read": "UPDATE mail SET read = 1 WHERE id = $1"
  },
  "validation": {
    "belongs_to_agent": "to_agent_id = current.uuid"
  }
}
```
