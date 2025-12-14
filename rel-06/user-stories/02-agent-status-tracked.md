# Agent Status Tracked

## Description

The agent registry tracks status ('running' or 'dead'). When an agent is killed, its status updates but the record remains for history.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> /fork

───────────────────────── agent [xyz789...] ─────────────────────────
Agent created

> /kill

───────────────────────── agent [abc123...] ─────────────────────────
Agent xyz789... terminated

> /agents
abc123... (running) - root
  xyz789... (dead)

> _
```

## Walkthrough

1. Agent xyz789... is created with `status = 'running'`
2. User runs `/kill` on agent xyz789...
3. Handler updates registry: `status = 'dead'`, `ended_at = now()`
4. Handler cleans up in-memory agent context
5. Handler switches to parent agent
6. `/agents` shows both agents, xyz789... marked as dead
7. Registry never deletes records - maintains full history

## Reference

```json
{
  "status_values": ["running", "dead"],
  "on_kill": {
    "update": "SET status = 'dead', ended_at = $1 WHERE uuid = $2"
  }
}
```
