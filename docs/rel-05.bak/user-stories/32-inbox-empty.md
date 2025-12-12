# Inbox Empty

## Description

User checks their inbox when there are no messages. A helpful message is displayed indicating the inbox is empty.

## Transcript

```text
───────── ↑- ←- [0/] →1/ ↓- ─────────────────────────
> /mail

Inbox for agent 0/:
  (no messages)

> _
```

## Walkthrough

1. User types `/mail` and presses Enter

2. REPL parses slash command, identifies `mail` command (inbox listing)

3. Command handler retrieves current agent's inbox

4. Inbox has `count == 0` (no messages)

5. Handler displays header: "Inbox for agent 0/:"

6. Handler displays placeholder: "  (no messages)"

7. No `[mail:N]` indicator in separator (unread count is 0)

8. User understands no messages are waiting
