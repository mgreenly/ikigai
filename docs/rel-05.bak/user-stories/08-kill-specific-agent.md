# Kill Specific Agent

## Description

User terminates a specific agent by ID using `/kill 1/`. User remains on current agent.

## Transcript

```text
───────────────────────── agent 0/ ─────────────────────────
> /kill 1/
Agent 1/ terminated

> _
```

## Walkthrough

1. User is on agent 0/

2. User types `/kill 1/` and presses Enter

3. REPL parses slash command with argument "1/"

4. Handler searches `repl->agents[]` for agent with id "1/"

5. Handler verifies target exists

6. Handler verifies target is not agent 0/ (protected)

7. Handler removes agent 1/ from `repl->agents[]` array

8. Handler frees agent 1/ resources via `talloc_free(agent)`

9. Handler displays confirmation: "Agent 1/ terminated"

10. User remains on agent 0/ (unchanged)

11. Agent indices recomputed for remaining agents
