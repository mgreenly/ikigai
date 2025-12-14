# Kill Cascade

## Description

User runs `/kill <uuid> --cascade` to terminate an agent and all its descendants.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> /agents
abc123... (running) - root
  xyz789... (running)
    child1... (running)
    child2... (running)
  def456... (running)

> /kill xyz789... --cascade

Agent xyz789... and 2 descendants terminated

> /agents
abc123... (running) - root
  xyz789... (dead)
    child1... (dead)
    child2... (dead)
  def456... (running)

> _
```

## Walkthrough

1. User is on root agent abc123...
2. xyz789... has two children (child1..., child2...)
3. User runs `/kill xyz789... --cascade`
4. Handler finds xyz789... and all descendants
5. Handler marks all as dead (depth-first order)
6. Handler cleans up all agent contexts
7. Handler displays count of terminated agents
8. All descendant histories preserved in database

## Reference

```json
{
  "command": "/kill <uuid> --cascade",
  "arguments": {
    "uuid": "target agent UUID",
    "--cascade": "include all descendants"
  },
  "traversal": "depth-first (children before parent)",
  "actions": {
    "marks_dead": "target + all descendants",
    "cleans_up": "all affected agent_ctx"
  }
}
```
