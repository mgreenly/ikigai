# Navigate to Parent

## Description

User presses Ctrl+↑ to navigate to the parent agent.

## Transcript

```text
───────────────────────── agent [child1...] ─────────────────────────
> _

[User presses Ctrl+↑]

───────────────────────── agent [parent...] ─────────────────────────
> _
```

## Walkthrough

1. User is on child1 (has parent_uuid)
2. User presses Ctrl+↑
3. Handler looks up parent_uuid
4. Handler finds parent agent in registry
5. Handler switches to parent agent
6. Child's state preserved (scrollback, input buffer)
7. If already at root (no parent), no action

## Reference

```json
{
  "navigation": {
    "Ctrl+↑": "go to parent agent"
  },
  "at_root": {
    "behavior": "no action (already at top)",
    "feedback": "optional visual indicator"
  }
}
```
