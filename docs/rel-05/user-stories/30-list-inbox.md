# List Inbox

## Description

User views their current agent's inbox using the `/mail` command. Messages are listed with unread messages first, showing sender, preview, and read status.

## Transcript

```text
───────── ↑- ←- [0/] →1/ ↓- ─────── [mail:2] ───────
> /mail

Inbox for agent 0/:
  #5 [unread] from 1/ - "Found 3 OAuth patterns worth considering: 1) Sil..."
  #4 [unread] from 2/ - "Build failed on test_auth.c line 42. Error: undef..."
  #3 from 1/ - "Starting research on OAuth patterns as requested..."
  #2 from 2/ - "Build complete, all 847 tests passing."
  #1 from 1/ - "Acknowledged. Beginning OAuth 2.0 research now."

> _
```

## Walkthrough

1. User types `/mail` and presses Enter

2. REPL parses slash command, identifies `mail` command with no subcommand (defaults to inbox listing)

3. Command handler retrieves current agent's inbox (`current_agent->inbox`)

4. Handler sorts messages: unread first, then by timestamp descending

5. Handler formats each message as:
   - `#ID` - message ID
   - `[unread]` - if not yet read
   - `from AGENT` - sender agent ID
   - `- "PREVIEW..."` - first ~50 chars of body, truncated with `...`

6. Handler displays header: "Inbox for agent {id}:"

7. Handler displays each message on its own line, indented

8. Messages truncated to terminal width minus indent

9. User sees overview of all messages, can use `/mail read ID` to see full content

10. Unread count in separator matches unread messages shown
