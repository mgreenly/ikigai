# Send Mail

## Description

User sends a message from their current agent to another agent using the `/mail send` command. The message is delivered to the recipient's inbox immediately.

## Transcript

```text
───────── ↑- ←- [0/] →1/ ↓- ─────────
> /mail send 1/ "Research OAuth 2.0 token refresh patterns"

Mail sent to agent 1/

> _
```

## Walkthrough

1. User types `/mail send 1/ "Research OAuth 2.0 token refresh patterns"` and presses Enter

2. REPL parses slash command, identifies `mail` command with `send` subcommand

3. Command handler extracts recipient (`1/`) and body (`Research OAuth...`)

4. Handler validates recipient agent exists in `repl->agents[]`

5. Handler validates body is non-empty

6. Handler creates `ik_mail_msg_t` with:
   - `from_agent_id`: current agent ID (`0/`)
   - `to_agent_id`: `1/`
   - `body`: message content
   - `timestamp`: current Unix time
   - `read`: false

7. Handler inserts message into database (mail table)

8. Handler adds message to recipient agent's inbox (`agents[1]->inbox`)

9. Handler increments recipient's `unread_count`

10. Handler displays confirmation: "Mail sent to agent 1/"

11. If recipient agent is IDLE with this being first unread, notification will be injected on next event loop

12. User remains on agent 0/, ready for next input
