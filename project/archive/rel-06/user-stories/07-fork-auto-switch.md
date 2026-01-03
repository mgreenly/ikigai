# Fork Auto Switch

## Description

After fork, the UI automatically switches to the child agent. The parent continues to exist and can be navigated to.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> /fork

───────────────────────── agent [xyz789...] ─────────────────────────
Forked from abc123...

> _

[User presses Ctrl+↑ to go to parent]

───────────────────────── agent [abc123...] ─────────────────────────
> _
```

## Walkthrough

1. User on agent abc123... runs `/fork`
2. Child agent xyz789... created
3. UI automatically switches to child
4. Separator shows child's UUID
5. User can navigate back to parent with Ctrl+↑
6. Parent's state preserved (scrollback, input buffer)
7. Both agents remain active

## Reference

```json
{
  "auto_switch": {
    "default": true,
    "switches_to": "newly created child",
    "preserves": "parent state (scrollback, input)",
    "navigation": "Ctrl+↑ returns to parent"
  }
}
```
