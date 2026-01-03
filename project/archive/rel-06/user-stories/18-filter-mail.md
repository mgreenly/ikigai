# Filter Mail

## Description

User runs `/filter-mail --from <uuid>` to filter inbox by sender.

## Transcript

```text
───────────────────────── agent [xyz789...] ─────────────────────────
> /filter-mail --from abc123...

Inbox (filtered by abc123..., 3 messages):

  [1] * from abc123... (2 min ago)
      "Please research OAuth 2.0 patterns"

  [5]   from abc123... (1 day ago)
      "Initial project setup instructions"

  [8]   from abc123... (3 days ago)
      "Welcome to the team"

> _
```

## Walkthrough

1. User types `/filter-mail --from abc123...`
2. Handler parses filter option
3. Handler queries mail table with additional filter:
   - `to_agent_id = current.uuid`
   - `from_agent_id = abc123...`
4. Handler orders by: unread first, then timestamp descending
5. Handler displays filtered inbox
6. Shows "(filtered by abc123...)" in header

## Reference

```json
{
  "command": "/filter-mail --from <uuid>",
  "arguments": {
    "--from": "filter by sender UUID"
  },
  "query": {
    "filter": "to_agent_id = current AND from_agent_id = filter",
    "order": "read ASC, timestamp DESC"
  },
  "future_filters": {
    "--unread": "show only unread messages",
    "--since": "messages after timestamp"
  }
}
```
