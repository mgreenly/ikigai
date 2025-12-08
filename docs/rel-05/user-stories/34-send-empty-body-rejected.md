# Send Empty Body Rejected

## Description

User attempts to send mail with an empty message body. The command is rejected with an error.

## Transcript

```text
───────── ↑- ←- [0/] →1/ ↓- ─────────────────────────
> /mail send 1/ ""

Error: Message body cannot be empty

> _
```

Also rejected:

```text
> /mail send 1/

Error: Usage: /mail send <agent-id> <message>

> _
```

## Walkthrough

1. User types `/mail send 1/ ""` and presses Enter

2. REPL parses slash command, identifies `mail` command with `send` subcommand

3. Command handler extracts recipient (`1/`) and body (empty string)

4. Handler validates body is non-empty after trimming whitespace

5. Body is empty, validation fails

6. Handler displays error: "Error: Message body cannot be empty"

7. No message created, no database write

8. User can retry with actual content

9. If body argument is missing entirely, show usage help instead
