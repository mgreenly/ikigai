# List Agents

## Description

User lists all agents with their current state using the `/agents` command.

## Transcript

```text
───────────────────────── agent 0/ ─────────────────────────
> /agents

Agents:
  0/ (current) - IDLE
  1/ - STREAMING
  2/ - EXECUTING_TOOL

> _
```

## Walkthrough

1. User types `/agents` and presses Enter

2. REPL parses slash command, dispatches to agents handler

3. Handler iterates `repl->agents[]` array

4. For each agent, handler formats line with:
   - Agent ID (e.g., "0/")
   - "(current)" marker if `idx == current_agent_idx`
   - State name (IDLE, SENDING, STREAMING, EXECUTING_TOOL)

5. Handler outputs formatted list to scrollback

6. User sees all agents and their states

7. User can identify which agent to switch to or kill
