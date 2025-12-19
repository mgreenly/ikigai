# Kill Self

## Description

User runs `/kill` to terminate the current agent. The agent is marked dead and UI switches to parent.

## Transcript

```text
───────────────────────── agent [xyz789...] ─────────────────────────
> I'm done with this task

> /kill

───────────────────────── agent [abc123...] ─────────────────────────
Agent xyz789... terminated

> _
```

## Walkthrough

1. User is on agent xyz789... (child of abc123...)
2. User types `/kill`
3. Handler updates registry: `status = 'dead'`, `ended_at = now()`
4. Handler cleans up in-memory agent context
5. Handler switches to parent agent abc123...
6. Parent displays "Agent xyz789... terminated"
7. Dead agent's history preserved in database
8. Dead agent can still be viewed but not interacted with

## Reference

```json
{
  "command": "/kill",
  "arguments": "none (kills current agent)",
  "actions": {
    "registry_update": "status = 'dead', ended_at = now()",
    "memory_cleanup": "free agent_ctx",
    "switch_to": "parent agent",
    "display": "termination message on parent"
  }
}
```
