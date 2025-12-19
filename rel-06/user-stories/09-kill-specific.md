# Kill Specific Agent

## Description

User runs `/kill <uuid>` to terminate a specific agent by UUID. This allows killing agents without switching to them.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> /agents
abc123... (running) - root
  xyz789... (running)
  def456... (running)

> /kill xyz789...

Agent xyz789... terminated

> /agents
abc123... (running) - root
  xyz789... (dead)
  def456... (running)

> _
```

## Walkthrough

1. User is on agent abc123... (parent)
2. User runs `/kill xyz789...`
3. Handler looks up agent by UUID
4. Handler verifies agent exists and is running
5. Handler updates registry: `status = 'dead'`
6. Handler cleans up target agent's memory
7. Handler displays "Agent xyz789... terminated"
8. User remains on current agent (no switch)

## Reference

```json
{
  "command": "/kill <uuid>",
  "arguments": {
    "uuid": "target agent UUID (partial match allowed)"
  },
  "validation": {
    "exists": "agent must exist in registry",
    "running": "agent must have status = 'running'",
    "not_current": "if killing current, switches to parent"
  }
}
```
