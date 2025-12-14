# Task: Agent Array in REPL Context

## Target
User Stories: 03-fork-creates-child.md, 19-agents-tree.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md
- project/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/repl.h (ik_repl_ctx_t - READ)
- src/repl_init.c (initialization - READ)
- src/agent.h (ik_agent_ctx_t - READ)

## Files to Create/Modify
- src/repl.h (MODIFY - add agents array fields and function declarations)
- src/repl.c (MODIFY - implement add/remove/find functions)
- src/repl_init.c (MODIFY - add initial agent to array)
- tests/unit/repl/repl_init_test.c (MODIFY - add agent array tests)

## Pre-read Tests (patterns)
- tests/unit/repl/repl_init_test.c (READ - existing test patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- registry-queries.md complete
- `ik_repl_ctx_t` has `current` pointer to single agent

## Task
Add an array to track all in-memory agents. This enables navigation between agents and the `/agents` command.

**Changes to ik_repl_ctx_t:**
```c
typedef struct ik_repl_ctx {
    // ... existing fields ...

    ik_agent_ctx_t *current;     // Current active agent (existing)
    ik_agent_ctx_t **agents;     // Array of all loaded agents
    size_t agent_count;          // Number of agents in array
    size_t agent_capacity;       // Allocated capacity
} ik_repl_ctx_t;
```

**Operations:**
```c
// Add agent to array (called after creation)
res_t ik_repl_add_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);

// Remove agent from array (called on kill)
res_t ik_repl_remove_agent(ik_repl_ctx_t *repl, const char *uuid);

// Find agent by UUID
ik_agent_ctx_t *ik_repl_find_agent(ik_repl_ctx_t *repl, const char *uuid);
```

## TDD Cycle

### Red
1. Update `src/repl.h` with new fields and function declarations

2. Create/update tests:
   - Test initial agent added to array on startup
   - Test agent_count = 1 after init
   - Test add_agent increases count
   - Test find_agent returns correct agent
   - Test find_agent returns NULL for unknown UUID
   - Test remove_agent decreases count
   - Test current pointer updated if current agent removed

3. Run `make check` - expect test failures

### Green
1. Update `src/repl.h` with fields
2. Implement functions in `src/repl.c`
3. Update `src/repl_init.c` to add initial agent to array
4. Run `make check` - expect pass

### Refactor
1. Verify talloc ownership (agents array child of repl_ctx)
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `ik_repl_ctx_t` has agents array
- Initial agent in array on startup
- Add/remove/find operations work
- Working tree is clean (all changes committed)
