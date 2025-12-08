# Task: Implement Agent Depth Calculation

**Target**: Agent-Spawned Sub-Agents

**Agent model**: haiku

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`

### Source patterns
- `src/repl/agent.h` - Agent context with hierarchy fields
- `src/repl/agent.c` - Agent implementation

## Pre-conditions

- Hierarchy fields exist in `ik_agent_ctx_t` (parent, children, etc.)

## Task

Implement `ik_agent_depth()` function that calculates agent depth by traversing parent chain:

```c
// Returns depth: 1 for top-level, 2 for first child, etc.
size_t ik_agent_depth(ik_agent_ctx_t *agent);
```

Depth calculation:
- Top-level agent (parent=NULL): depth 1
- First child (0/0): depth 2
- Grandchild (0/0/0): depth 3
- Max allowed: depth 4

## TDD Cycle

### Red
Write tests:
- Top-level agent returns depth 1
- Child of top-level returns depth 2
- Grandchild returns depth 3
- Great-grandchild returns depth 4

### Green
Implement by counting parent traversals + 1.

### Verify
`make check` passes.

## Post-conditions

- `ik_agent_depth()` correctly calculates depth
- All tests pass
