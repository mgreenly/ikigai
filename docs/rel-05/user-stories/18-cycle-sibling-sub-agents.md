# Cycle Sibling Sub-Agents

## Description

User cycles through sibling sub-agents using Ctrl+Left and Ctrl+Right. Navigation wraps around at boundaries.

## Transcript

```text
───────── ↑0/ ←0/2 [0/0] →0/1 ↓- ─────────
> [Ctrl+Right]

───────── ↑0/ ←0/0 [0/1] →0/2 ↓- ─────────
> [Ctrl+Right]

───────── ↑0/ ←0/1 [0/2] →0/0 ↓- ─────────
> [Ctrl+Right]

───────── ↑0/ ←0/2 [0/0] →0/1 ↓- ─────────
> _
```

## Walkthrough

1. User is on agent 0/0 with siblings [0/0, 0/1, 0/2]

2. User presses Ctrl+Right

3. Input parser recognizes Ctrl+Right sequence

4. Parser emits ACTION_SWITCH_NEXT action

5. REPL handler finds siblings (agents with same parent)

6. Handler calculates next sibling: 0/1

7. Handler switches to agent 0/1

8. User presses Ctrl+Right again, switches to 0/2

9. User presses Ctrl+Right again

10. Handler wraps around: next after 0/2 is 0/0

11. User is back on 0/0 (circular navigation)

12. Same logic applies for Ctrl+Left (backward wrap)
