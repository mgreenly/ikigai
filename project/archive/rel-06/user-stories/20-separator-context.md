# Separator Navigation Context

## Description

The separator line shows navigation context: available directions (↑↓←→) and current agent info.

## Transcript

```text
───────── ↑abc... ←xyz... [def123...] →ghi... ↓2 ─────────
> _
```

## Walkthrough

1. Separator rendered at top of agent view
2. Shows navigation indicators:
   - `↑abc...` - parent agent UUID (if exists)
   - `←xyz...` - previous sibling (if exists)
   - `[def123...]` - current agent UUID
   - `→ghi...` - next sibling (if exists)
   - `↓2` - number of children (if any)
3. Missing directions shown as `-`:
   - `↑-` means no parent (root agent)
   - `↓-` means no children
4. Truncated UUIDs for space (first 6 chars)
5. Updates on agent switch

## Reference

```json
{
  "separator_format": "↑{parent} ←{prev} [{current}] →{next} ↓{children}",
  "indicators": {
    "↑": "parent UUID or '-' if root",
    "←": "previous sibling UUID or '-'",
    "→": "next sibling UUID or '-'",
    "↓": "child count or '-' if none"
  },
  "uuid_display": {
    "truncate": 6,
    "current": "full or truncated based on width"
  }
}
```
