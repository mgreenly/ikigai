# Preserved Input on Switch

## Description

Partial input is preserved when switching agents. Returning to an agent shows the incomplete text.

## Transcript

```text
───────────────────────── agent 0/ ─────────────────────────
> Add error handling for [Ctrl+Right]

───────────────────────── agent 1/ ─────────────────────────
> [Ctrl+Left]

───────────────────────── agent 0/ ─────────────────────────
> Add error handling for _
```

## Walkthrough

1. User is on agent 0/

2. User types "Add error handling for " (partial input, not submitted)

3. User presses Ctrl+Right to switch to agent 1/

4. Agent 0/'s input buffer retains "Add error handling for "

5. Agent 1/'s input buffer is displayed (empty or different content)

6. User presses Ctrl+Left to switch back to agent 0/

7. Agent 0/'s input buffer is displayed with preserved text

8. User sees "Add error handling for " still in input

9. User can continue typing and submit

10. Each agent owns its own `ik_input_buffer_t`
