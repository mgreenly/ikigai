# Agents Tree Display

## Description

User runs `/agents` to see a tree view of all agents and their hierarchy.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> /agents

Agent Hierarchy:

* abc123... (running) - root
    xyz789... (running)
      child1... (running)
      child2... (dead)
    def456... (running)
    ghi012... (dead)

4 running, 2 dead

> _
```

## Walkthrough

1. User types `/agents`
2. Handler queries all agents from registry
3. Handler builds tree structure from parent_uuid relationships
4. Handler formats tree display:
   - `*` marks current agent
   - Indentation shows hierarchy
   - Status in parentheses
   - "root" label for root agent
5. Handler shows summary count at bottom
6. Dead agents shown but grayed/dimmed

## Reference

```json
{
  "command": "/agents",
  "display": {
    "format": "tree with indentation",
    "current_marker": "*",
    "status_display": "(running) or (dead)",
    "root_label": "- root"
  },
  "query": {
    "all_agents": "SELECT uuid, parent_uuid, status FROM agents",
    "order": "parent_uuid, created_at"
  }
}
```
