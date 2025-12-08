# Switch Wrap Forward

## Description

User presses Ctrl+Right on the last agent. Navigation wraps to agent 0/.

## Transcript

```text
───────────────────────── agent 2/ ─────────────────────────
> [Ctrl+Right]

───────────────────────── agent 0/ ─────────────────────────
> _
```

## Walkthrough

1. User is on agent 2/ (last agent, index 2 of 3 agents)

2. User presses Ctrl+Right

3. Input parser recognizes escape sequence for Ctrl+Right arrow

4. REPL calculates target index: `(2 + 1) % 3 = 0`

5. Target is agent 0/ (wrap around)

6. REPL updates `current_agent_idx` to 0

7. REPL attaches agent 0/'s layer_cake and input_buffer

8. REPL triggers full redraw

9. Agent 0/'s scrollback now visible

10. Separator shows "agent 0/"
