# Read Mail

## Description

User reads a specific message from their inbox using `/mail read ID`. The full message is displayed and automatically marked as read.

## Transcript

```text
───────── ↑- ←- [0/] →1/ ↓- ─────── [mail:2] ───────
> /mail read 5

From: 1/
Time: 2 minutes ago

Found 3 OAuth patterns worth considering:

1) Silent refresh - Refresh token in background before expiry. Requires
   background timer. Transparent to user.

2) Retry with refresh - Catch 401, refresh token, retry original request.
   Simpler implementation, slight latency on token expiry.

3) Token rotation - Each refresh returns new refresh token. Most secure
   but requires careful state management.

Recommend pattern #2 for our use case. Want me to draft implementation?

───────── ↑- ←- [0/] →1/ ↓- ─────── [mail:1] ───────
> _
```

## Walkthrough

1. User types `/mail read 5` and presses Enter

2. REPL parses slash command, identifies `mail` command with `read` subcommand

3. Command handler extracts message ID (`5`)

4. Handler looks up message in current agent's inbox by ID

5. If message not found, displays error: "Message #5 not found"

6. If found, handler displays full message:
   - `From: {sender_agent_id}`
   - `Time: {relative_time}` (e.g., "2 minutes ago", "yesterday")
   - Blank line
   - Full message body (no truncation)

7. Handler marks message as read (`msg->read = true`)

8. Handler decrements agent's `unread_count`

9. Handler updates database (set `read = 1` for this message)

10. Separator re-renders with updated unread count (`[mail:1]` instead of `[mail:2]`)

11. If this was the triggering message for a pending notification, clear `mail_notification_pending` flag

12. User can now respond or continue working
