# Navigate Siblings

## Description

User presses Ctrl+← or Ctrl+→ to cycle through sibling agents (agents with the same parent).

## Transcript

```text
───────────────────────── agent [child1...] ─────────────────────────
> _

[User presses Ctrl+→]

───────────────────────── agent [child2...] ─────────────────────────
> _

[User presses Ctrl+→]

───────────────────────── agent [child3...] ─────────────────────────
> _

[User presses Ctrl+→]

───────────────────────── agent [child1...] ─────────────────────────
> _
```

## Walkthrough

1. Parent has three children: child1, child2, child3
2. User is on child1, presses Ctrl+→
3. Handler finds siblings (same parent_uuid)
4. Handler finds current position in sibling list
5. Handler switches to next sibling (child2)
6. Ctrl+→ wraps around: child3 → child1
7. Ctrl+← goes in reverse order
8. If no siblings, no action

## Reference

```json
{
  "navigation": {
    "Ctrl+→": "next sibling (wraps)",
    "Ctrl+←": "previous sibling (wraps)"
  },
  "siblings": {
    "definition": "agents with same parent_uuid",
    "ordering": "by created_at timestamp"
  }
}
```
