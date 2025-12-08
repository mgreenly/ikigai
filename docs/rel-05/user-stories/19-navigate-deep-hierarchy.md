# Navigate Deep Hierarchy

## Description

User navigates through multiple levels of nested sub-agents using Ctrl+Up and Ctrl+Down.

## Transcript

```text
───────── ↑- ←1/ [0/] →1/ ↓0/0 ─────────
> [Ctrl+Down]

───────── ↑0/ ←0/0 [0/0] →0/0 ↓0/0/0 ─────────
> [Ctrl+Down]

───────── ↑0/0 ←0/0/0 [0/0/0] →0/0/0 ↓- ─────────
> [Ctrl+Up]

───────── ↑0/ ←0/0 [0/0] →0/0 ↓0/0/0 ─────────
> [Ctrl+Up]

───────── ↑- ←1/ [0/] →1/ ↓0/0 ─────────
> _
```

## Walkthrough

1. User starts on agent 0/ (top-level)

2. Agent 0/ has child 0/0, which has child 0/0/0

3. User presses Ctrl+Down, navigates to 0/0

4. User presses Ctrl+Down again, navigates to 0/0/0

5. User is now at depth 3 (0/0/0)

6. Separator shows `↓-` (no children, at or near max depth)

7. User presses Ctrl+Up, navigates back to 0/0

8. User presses Ctrl+Up again, navigates back to 0/

9. User is back at top-level agent 0/

10. Full round-trip through hierarchy complete
