# Switch Wrap Backward

## Description

User presses Ctrl+Left on agent 0/. Navigation wraps to the last agent.

## Transcript

```text
───────────────────────── agent 0/ ─────────────────────────
> [Ctrl+Left]

───────────────────────── agent 2/ ─────────────────────────
> _
```

## Walkthrough

1. User is on agent 0/ (first agent, index 0 of 3 agents)

2. User presses Ctrl+Left

3. Input parser recognizes escape sequence for Ctrl+Left arrow

4. REPL calculates target index: `0 == 0 ? 3 - 1 : 0 - 1 = 2`

5. Target is agent 2/ (wrap around to last)

6. REPL updates `current_agent_idx` to 2

7. REPL attaches agent 2/'s layer_cake and input_buffer

8. REPL triggers full redraw

9. Agent 2/'s scrollback now visible

10. Separator shows "agent 2/"
