# Cannot Type in Sub-Agent

## Description

When viewing a sub-agent, user input is disabled. Sub-agents are autonomous and do not accept human interaction.

## Transcript

```text
───────── ↑0/ ←0/0 [0/0] →0/0 ↓- ─────────

[Sub-agent working on delegated task...]

I've found three issues in the codebase...

> _  [autonomous agent - input disabled]

[User types "hello"]
[Nothing happens - keystrokes ignored]

[User presses Enter]
[Nothing happens - submit ignored]
```

## Walkthrough

1. User navigates to sub-agent 0/0 (via Ctrl+Down)

2. Sub-agent is working (spawned by parent via tool)

3. Input buffer displays but is visually marked as disabled

4. User attempts to type characters

5. Input parser receives keystrokes

6. Handler checks if current agent is a sub-agent (is_sub_agent == true)

7. Handler ignores character input for sub-agents

8. User attempts to press Enter to submit

9. Handler ignores submit action for sub-agents

10. Only navigation keys work (Ctrl+Arrow to move around)

11. Special commands may work: `/kill` to terminate sub-agent

12. User can observe but not direct the sub-agent's work

13. This preserves the delegation contract (parent controls sub-agent's task)
