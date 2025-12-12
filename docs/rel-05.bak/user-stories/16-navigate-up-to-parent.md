# Navigate Up to Parent

## Description

User navigates from sub-agent back to parent agent using Ctrl+Up.

## Transcript

```text
───────── ↑0/ ←0/1 [0/0] →0/1 ↓- ─────────
> [Ctrl+Up]

───────── ↑- ←1/ [0/] →1/ ↓0/0 ─────────
> _
```

## Walkthrough

1. User is on agent 0/0 (a sub-agent of 0/)

2. Separator shows `↑0/` indicating parent is agent 0/

3. User presses Ctrl+Up

4. Input parser recognizes Ctrl+Up sequence

5. Parser emits ACTION_NAVIGATE_UP action

6. REPL handler checks if current agent has parent

7. Current agent has parent (agent->parent != NULL)

8. Handler identifies parent: agent 0/

9. Handler performs lazy reflow on 0/ if needed

10. Handler switches to agent 0/ (update current_agent_idx, attach layer_cake and input_buffer)

11. Separator re-renders showing new navigation context

12. User is now on agent 0/, can see its scrollback
