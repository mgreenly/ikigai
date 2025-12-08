# Task: Update /agents Command for Tree Display

**Target**: Agent-Spawned Sub-Agents

**Agent model**: sonnet

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`

### Docs
- `docs/rel-05/backlog/agent-spawned-sub-agents.md` - Tree display

### Source patterns
- `src/repl/commands.c` - /agents command

## Pre-conditions

- Agent hierarchy exists
- /agents command lists agents

## Task

Update /agents to show tree hierarchy using Linux `tree` style:

```
/agents

0/ IDLE
├── 0/0 STREAMING
│   └── 0/0/0 EXECUTING_TOOL
├── 0/1 IDLE
└── 0/2 (current) IDLE
1/ IDLE
└── 1/0 IDLE
2/ IDLE
```

Characters:
- `├──` Branch with siblings below
- `└──` Last child (no siblings below)
- `│` Vertical continuation line

Include:
- Agent ID
- State (IDLE, STREAMING, EXECUTING_TOOL)
- `(current)` marker for current agent

## TDD Cycle

### Red
Write tests:
- Single top-level agent (no tree chars)
- Agent with one child
- Agent with multiple children
- Nested children (grandchildren)
- Current agent marker shown

### Green
Implement tree traversal with proper indentation and branch characters.

### Verify
`make check` passes.

## Post-conditions

- /agents shows tree hierarchy
- Correct tree characters
- All tests pass
