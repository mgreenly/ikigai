# Kill Current Agent

## Description

User terminates the current agent using `/kill`. REPL switches to next available agent before cleanup.

## Transcript

```text
───────────────────────── agent 1/ ─────────────────────────
> /kill
Agent 1/ terminated

───────────────────────── agent 2/ ─────────────────────────
> _
```

## Walkthrough

1. User is on agent 1/ (not agent 0/)

2. User types `/kill` and presses Enter

3. REPL parses slash command, dispatches to kill handler

4. Handler identifies target as current agent (1/)

5. Handler verifies target is not agent 0/ (protected)

6. Handler calculates next agent to switch to (agent 2/)

7. Handler switches to agent 2/ first (attach layer_cake, input_buffer)

8. Handler removes agent 1/ from `repl->agents[]` array

9. Handler frees agent 1/ resources via `talloc_free(agent)`

10. Handler displays confirmation: "Agent 1/ terminated"

11. User is now on agent 2/
