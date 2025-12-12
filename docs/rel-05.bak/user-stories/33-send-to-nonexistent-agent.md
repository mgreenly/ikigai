# Send to Nonexistent Agent

## Description

User attempts to send mail to an agent that doesn't exist. An error message is displayed and no mail is sent.

## Transcript

```text
───────── ↑- ←- [0/] →1/ ↓- ─────────────────────────
> /mail send 99/ "Hello there"

Error: Agent 99/ not found

> _
```

## Walkthrough

1. User types `/mail send 99/ "Hello there"` and presses Enter

2. REPL parses slash command, identifies `mail` command with `send` subcommand

3. Command handler extracts recipient (`99/`) and body

4. Handler searches `repl->agents[]` for agent with ID `99/`

5. Agent not found in array

6. Handler displays error: "Error: Agent 99/ not found"

7. No message created, no database write

8. User remains on current agent, can correct and retry

9. Error displayed inline in scrollback (not a modal or popup)
