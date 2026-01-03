# Send Mail

## Description

User sends a message from current agent to another agent using `/send <uuid> "message"`.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> /send xyz789... "Please research OAuth 2.0 patterns"

Mail sent to xyz789...

> _
```

## Walkthrough

1. User types `/send xyz789... "Please research OAuth 2.0 patterns"`
2. Handler parses command: recipient UUID, message body
3. Handler validates recipient exists in registry
4. Handler validates message body is non-empty
5. Handler creates mail record:
   - `from_agent_id`: current agent UUID
   - `to_agent_id`: xyz789...
   - `body`: message content
   - `timestamp`: current time
   - `read`: false
6. Handler inserts into `mail` table
7. Handler displays "Mail sent to xyz789..."
8. Recipient can check mail with `/check-mail`

## Reference

```json
{
  "command": "/send <uuid> \"message\"",
  "arguments": {
    "uuid": "recipient agent UUID",
    "message": "quoted message body"
  },
  "validation": {
    "recipient_exists": true,
    "body_non_empty": true
  },
  "mail_record": {
    "from_agent_id": "sender UUID",
    "to_agent_id": "recipient UUID",
    "body": "message text",
    "timestamp": "Unix epoch",
    "read": false
  }
}
```
