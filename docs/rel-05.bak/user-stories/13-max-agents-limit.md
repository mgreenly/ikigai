# Max Agents Limit

## Description

User attempts to spawn more than 20 agents. The command fails with an error message.

## Transcript

```text
───────────────────────── agent 19/ ─────────────────────────
> /spawn
Error: Maximum agents (20) reached

> _
```

## Walkthrough

1. User has 20 agents active (0/ through 19/)

2. User types `/spawn` and presses Enter

3. REPL parses slash command, dispatches to spawn handler

4. Handler checks `repl->agent_count >= 20`

5. Limit exceeded, handler returns error

6. Error displayed: "Maximum agents (20) reached"

7. No new agent created

8. User remains on current agent

9. User must `/kill` an agent before spawning new one

10. Limit prevents unbounded memory growth
