# Check Mail

## Description

User runs `/check-mail` to see a list of messages in the current agent's inbox.

## Transcript

```text
───────────────────────── agent [xyz789...] ─────────────────────────
> /check-mail

Inbox (2 messages, 1 unread):

  [1] * from abc123... (2 min ago)
      "Please research OAuth 2.0 patterns"

  [2]   from def456... (1 hour ago)
      "Here's the database schema you requested"

> _
```

## Walkthrough

1. User types `/check-mail`
2. Handler queries mail table for `to_agent_id = current.uuid`
3. Handler orders by: unread first, then timestamp descending
4. Handler formats inbox display:
   - Message ID (for `/read-mail`)
   - `*` marker for unread messages
   - Sender UUID
   - Relative timestamp
   - Message preview (truncated)
5. Handler displays inbox summary
6. Messages remain unread until explicitly read

## Reference

```json
{
  "command": "/check-mail",
  "query": {
    "filter": "to_agent_id = current.uuid",
    "order": "read ASC, timestamp DESC"
  },
  "display": {
    "unread_marker": "*",
    "preview_length": 50,
    "timestamp": "relative (2 min ago, 1 hour ago)"
  }
}
```
