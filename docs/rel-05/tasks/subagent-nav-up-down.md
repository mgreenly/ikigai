# Task: Implement Ctrl+Up/Down Navigation

**Target**: Agent-Spawned Sub-Agents

**Agent model**: sonnet

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`

### Docs
- `docs/rel-05/backlog/agent-spawned-sub-agents.md` - Navigation model

### Source patterns
- `src/input/input_parser.c` - Input action parsing
- `src/repl/repl.c` - Agent switching logic

## Pre-conditions

- Ctrl+Left/Right navigation works (sibling cycling)
- Agent hierarchy (parent/children) exists

## Task

Add Ctrl+Up and Ctrl+Down navigation for hierarchy traversal:

**Ctrl+Up**: Navigate to parent agent
- If at top-level (parent=NULL): no-op
- Otherwise: switch to parent agent

**Ctrl+Down**: Navigate to first child
- If no children: no-op
- Otherwise: switch to children[0]

Add input actions:
```c
IK_INPUT_AGENT_PARENT   // Ctrl+Up
IK_INPUT_AGENT_CHILD    // Ctrl+Down
```

## TDD Cycle

### Red
Write tests:
- Ctrl+Up from child switches to parent
- Ctrl+Up from top-level is no-op
- Ctrl+Down to first child works
- Ctrl+Down with no children is no-op

### Green
Add input action parsing and handler logic.

### Verify
`make check` passes.

## Post-conditions

- Ctrl+Up/Down navigate hierarchy
- No-op when direction unavailable
- All tests pass
