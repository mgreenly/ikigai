# Navigate to Child

## Description

User presses Ctrl+↓ to navigate to a child agent. If multiple children exist, navigates to the most recently created running child.

## Transcript

```text
───────────────────────── agent [parent...] ─────────────────────────
> _

[User presses Ctrl+↓]

───────────────────────── agent [child2...] ─────────────────────────
> _
```

## Walkthrough

1. User is on parent with children: child1, child2 (child2 newer)
2. User presses Ctrl+↓
3. Handler queries children of current agent
4. Handler filters for `status = 'running'`
5. Handler selects most recently created child
6. Handler switches to child2
7. If no children, no action
8. If no running children, no action

## Reference

```json
{
  "navigation": {
    "Ctrl+↓": "go to child agent"
  },
  "child_selection": {
    "filter": "status = 'running'",
    "order": "most recently created",
    "fallback": "no action if no running children"
  }
}
```
