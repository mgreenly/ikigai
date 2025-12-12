# Kill Parent Cascades

## Description

When killing an agent that has descendants, all descendants are killed first (depth-first), then the parent is killed.

## Transcript

```text
/agents

0/ (current) IDLE
├── 0/0 STREAMING
│   └── 0/0/0 IDLE
└── 0/1 IDLE
1/ IDLE

───────── ↑- ←1/ [0/] →1/ ↓0/0 ─────────
> /kill

Killing agent 0/0/0...
Killing agent 0/0...
Killing agent 0/1...
Killing agent 0/...
Agent 0/ and 3 descendants terminated

───────── ↑- ←1/ [1/] →1/ ↓- ─────────
> _
```

## Walkthrough

1. User is on agent 0/ which has children 0/0 and 0/1

2. Agent 0/0 has child 0/0/0 (grandchild of 0/)

3. User types `/kill` (kill current agent)

4. Handler verifies 0/ is not agent 0/ ... wait, it IS agent 0/

5. **Correction**: Agent 0/ is protected, cannot be killed

6. Let's say user is on agent 2/ with descendants instead:

```text
> /kill

Killing agent 2/0/0...
Killing agent 2/0...
Killing agent 2/1...
Killing agent 2/...
Agent 2/ and 3 descendants terminated
```

7. Handler identifies all descendants via depth-first traversal

8. Kill order: 2/0/0 (deepest), then 2/0, then 2/1, then 2/

9. Each agent's resources freed via talloc_free

10. Agents removed from parent's children arrays

11. User switched to next available agent before cleanup

12. Confirmation shows count of terminated agents
