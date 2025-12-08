# Navigate Up From Top Level

## Description

User attempts to navigate up from a top-level agent. Nothing happens (no-op).

## Transcript

```text
───────── ↑- ←1/ [0/] →1/ ↓0/0 ─────────
> [Ctrl+Up]

───────── ↑- ←1/ [0/] →1/ ↓0/0 ─────────
> _
```

## Walkthrough

1. User is on agent 0/ (a top-level agent)

2. Separator shows `↑-` indicating no upward navigation available

3. User presses Ctrl+Up

4. Input parser recognizes Ctrl+Up sequence

5. Parser emits ACTION_NAVIGATE_UP action

6. REPL handler checks if current agent has parent

7. Current agent has no parent (agent->parent == NULL, top-level)

8. Handler does nothing (no-op)

9. User remains on agent 0/

10. No visual change, no error message
