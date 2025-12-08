# Agents Tree Display

## Description

The `/agents` command displays all agents in a tree hierarchy using Linux `tree` command style characters.

## Transcript

```text
───────── ↑0/ ←0/1 [0/0] →0/1 ↓0/0/0 ─────────
> /agents

0/ IDLE
├── 0/0 (current) STREAMING
│   └── 0/0/0 EXECUTING_TOOL
└── 0/1 IDLE
1/ IDLE
└── 1/0 IDLE
2/ IDLE

> _
```

## Walkthrough

1. User types `/agents` command

2. Handler collects all agents from repl->agents[] and their children

3. Handler builds tree structure from parent-child relationships

4. Top-level agents listed first (0/, 1/, 2/)

5. Children indented under parents with tree characters

6. Tree characters used:
   - `├──` for child with siblings below
   - `└──` for last child (no siblings below)
   - `│` for vertical continuation line

7. Each agent shows:
   - Agent ID (0/, 0/0, etc.)
   - `(current)` marker if this is current agent
   - State: IDLE, STREAMING, or EXECUTING_TOOL

8. Indentation increases with depth

9. Display is sorted: top-level by ID, children by ID within parent

10. User gets complete view of agent hierarchy and states
