# Switch to Next Agent

## Description

User switches to the next agent using Ctrl+Right. The new agent's scrollback and input buffer become active.

## Transcript

```text
───────────────────────── agent 0/ ─────────────────────────
> [Ctrl+Right]

───────────────────────── agent 1/ ─────────────────────────
> _
```

## Walkthrough

1. User presses Ctrl+Right

2. Input parser recognizes escape sequence for Ctrl+Right arrow

3. REPL calculates target index: `(current_idx + 1) % agent_count`

4. REPL checks if target agent needs reflow (lazy SIGWINCH handling)

5. REPL updates `current_agent_idx` to target

6. REPL attaches target agent's layer_cake to render context

7. REPL attaches target agent's input_buffer to input handling

8. REPL triggers full redraw of terminal

9. Target agent's scrollback, separator, and input buffer now visible

10. Separator shows new agent ID
