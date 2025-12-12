# Navigate Down to Child

## Description

User navigates from parent agent to first child sub-agent using Ctrl+Down.

## Transcript

```text
───────── ↑- ←1/ [0/] →1/ ↓0/0 ─────────
> [Ctrl+Down]

───────── ↑0/ ←0/1 [0/0] →0/1 ↓- ─────────
> _
```

## Walkthrough

1. User is on agent 0/ which has children [0/0, 0/1]

2. User presses Ctrl+Down

3. Input parser recognizes Ctrl+Down sequence

4. Parser emits ACTION_NAVIGATE_DOWN action

5. REPL handler checks if current agent has children

6. Current agent has children, so handler proceeds

7. Handler identifies first child: agent 0/0

8. Handler performs lazy reflow on 0/0 if needed (SIGWINCH handling)

9. Handler switches to agent 0/0 (update current_agent_idx, attach layer_cake and input_buffer)

10. Separator re-renders showing new navigation context

11. User is now on agent 0/0, can see its scrollback
