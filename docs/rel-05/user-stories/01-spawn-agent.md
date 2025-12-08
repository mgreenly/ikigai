# Spawn Agent

## Description

User creates a new agent using the `/spawn` command. The new agent is created and user automatically switches to it.

## Transcript

```text
───────────────────────── agent 0/ ─────────────────────────
> /spawn

───────────────────────── agent 1/ ─────────────────────────
Agent 1/ created

> _
```

## Walkthrough

1. User types `/spawn` and presses Enter

2. REPL parses slash command, dispatches to spawn handler

3. Handler checks agent count < 20 (max limit)

4. Handler allocates new `ik_agent_ctx_t`

5. Handler generates agent_id: `"1/"` (next_agent_serial++)

6. Handler initializes agent state (scrollback, input_buffer, layer_cake, conversation)

7. Handler adds agent to `repl->agents[]` array

8. Handler switches to new agent (update current_agent_idx, attach layer_cake and input_buffer)

9. Handler displays confirmation: "Agent 1/ created"

10. User is now on agent 1/ with fresh scrollback
