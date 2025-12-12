# Navigate Down No Children

## Description

User attempts to navigate down from an agent with no children. Nothing happens (no-op).

## Transcript

```text
───────── ↑0/ ←0/0 [0/1] →0/0 ↓- ─────────
> [Ctrl+Down]

───────── ↑0/ ←0/0 [0/1] →0/0 ↓- ─────────
> _
```

## Walkthrough

1. User is on agent 0/1 which has no children

2. Separator shows `↓-` indicating no downward navigation available

3. User presses Ctrl+Down

4. Input parser recognizes Ctrl+Down sequence

5. Parser emits ACTION_NAVIGATE_DOWN action

6. REPL handler checks if current agent has children

7. Current agent has no children (child_count == 0)

8. Handler does nothing (no-op)

9. User remains on agent 0/1

10. No visual change, no error message
