# Task: Implement Child Array Management

**Target**: Agent-Spawned Sub-Agents

**Agent model**: sonnet

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`
- `.agents/skills/patterns/arena-allocator.md`

### Docs
- `docs/memory.md` - talloc patterns

### Source patterns
- `src/repl/agent.h` - Agent context
- `src/repl/agent.c` - Agent implementation

## Pre-conditions

- Hierarchy fields exist in `ik_agent_ctx_t`
- Depth calculation works

## Task

Implement functions to manage agent's children array:

```c
// Add child to parent's children array
res_t ik_agent_add_child(ik_agent_ctx_t *parent, ik_agent_ctx_t *child);

// Remove child from parent's children array
res_t ik_agent_remove_child(ik_agent_ctx_t *parent, ik_agent_ctx_t *child);

// Get child count
size_t ik_agent_child_count(ik_agent_ctx_t *agent);
```

Child is talloc-parented to parent agent context. Dynamic array grows as needed.

## TDD Cycle

### Red
Write tests:
- Add first child to empty parent
- Add multiple children
- Remove child from middle
- Remove last child
- Child count reflects actual children

### Green
Implement with dynamic array (realloc pattern via talloc_realloc).

### Verify
`make check` passes.

## Post-conditions

- Child array management functions work correctly
- Children are properly talloc-parented
- All tests pass
