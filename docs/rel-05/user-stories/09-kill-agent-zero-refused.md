# Kill Agent Zero Refused

## Description

User attempts to kill agent 0/ but the command is refused. Agent 0/ is protected and cannot be terminated.

## Transcript

```text
───────────────────────── agent 0/ ─────────────────────────
> /kill 0/
Error: Cannot kill agent 0/

> _
```

## Walkthrough

1. User types `/kill 0/` and presses Enter

2. REPL parses slash command with argument "0/"

3. Handler identifies target as agent 0/

4. Handler checks if target is agent 0/

5. Handler returns error: "Cannot kill agent 0/"

6. Error displayed in scrollback

7. No state changes occur

8. Agent 0/ remains active

9. User remains on current agent
