# Task: Add Hierarchy Fields to Agent Context

**Target**: Agent-Spawned Sub-Agents

**Agent model**: sonnet

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`
- `.agents/skills/naming.md`
- `.agents/skills/patterns/context-struct.md`

### Source patterns
- `src/repl/agent.h` - Current agent context structure

### Test patterns
- `tests/unit/test_agent.c` - Existing agent tests

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- Multi-Agent Implementation complete (agents[] array exists)
- `ik_agent_ctx_t` exists with identity fields (agent_id, numeric_id)

## Task

Extend `ik_agent_ctx_t` with hierarchy fields to support parent-child relationships:

```c
// NEW: Hierarchy
struct ik_agent_ctx_t *parent;     // NULL for top-level agents
struct ik_agent_ctx_t **children;  // Dynamic array of children
size_t child_count;
size_t child_capacity;
size_t next_child_serial;          // For generating 0/0, 0/1, 0/2

// NEW: Sub-agent completion
bool is_sub_agent;                 // true if spawned by tool (not /spawn)
char *final_response;              // Captured when sub-agent completes
bool completed;                    // true when would-wait-for-human
```

Initialize all new fields in agent creation. Top-level agents have `parent = NULL` and `is_sub_agent = false`.

## TDD Cycle

### Red
Write tests verifying:
- New fields initialized correctly for top-level agent
- `parent` is NULL for top-level agents
- `children` array starts empty
- `is_sub_agent` is false for /spawn-created agents

### Green
Add fields to struct, update initialization code.

### Verify
`make check` passes with 100% coverage.

## Post-conditions

- `ik_agent_ctx_t` contains hierarchy fields
- Top-level agents initialize with parent=NULL, is_sub_agent=false
- All tests pass
- Working tree is clean (all changes committed)
