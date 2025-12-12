# Separator Navigation Context

## Description

The separator line shows navigation context: current agent and where each Ctrl+Arrow direction will navigate.

## Transcript

At top-level with children:
```text
───────── ↑- ←2/ [0/] →1/ ↓0/0 ─────────
```

At sub-agent with siblings and children:
```text
───────── ↑0/ ←0/2 [0/1] →0/0 ↓0/1/0 ─────────
```

At leaf sub-agent (no children):
```text
───────── ↑0/0 ←0/0/1 [0/0/0] →0/0/1 ↓- ─────────
```

Single top-level agent, no children:
```text
───────── ↑- [0/] ↓- ─────────
```

## Walkthrough

1. Separator renders between scrollback and input

2. Current agent shown in brackets: `[0/1]`

3. Up arrow shows parent or `-` if top-level: `↑0/` or `↑-`

4. Left arrow shows previous sibling: `←0/0`

5. Right arrow shows next sibling: `→0/2`

6. Down arrow shows first child or `-` if none: `↓0/1/0` or `↓-`

7. Siblings wrap circularly (last→first, first→last)

8. If only one agent at level, left/right arrows may be omitted

9. Format adjusts to terminal width (dashes fill remaining space)

10. Navigation context updates immediately on agent switch

11. User always knows:
    - Which agent they're viewing
    - What Ctrl+Arrow will do in each direction

12. Dash (`-`) clearly indicates "no navigation available"
