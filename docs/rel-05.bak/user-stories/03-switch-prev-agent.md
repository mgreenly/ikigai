# Switch to Previous Agent

## Description

User switches to the previous agent using Ctrl+Left. The previous agent's scrollback and input buffer become active.

## Transcript

```text
───────────────────────── agent 1/ ─────────────────────────
> [Ctrl+Left]

───────────────────────── agent 0/ ─────────────────────────
> _
```

## Walkthrough

1. User presses Ctrl+Left

2. Input parser recognizes escape sequence for Ctrl+Left arrow

3. REPL calculates target index: `current_idx == 0 ? agent_count - 1 : current_idx - 1`

4. REPL checks if target agent needs reflow (lazy SIGWINCH handling)

5. REPL updates `current_agent_idx` to target

6. REPL attaches target agent's layer_cake to render context

7. REPL attaches target agent's input_buffer to input handling

8. REPL triggers full redraw of terminal

9. Target agent's scrollback, separator, and input buffer now visible

10. Separator shows new agent ID
