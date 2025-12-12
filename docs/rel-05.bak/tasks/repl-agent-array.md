# Task: Convert REPL to Agent Array

## Target
User Stories: All (foundational infrastructure for multi-agent)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/ddd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/patterns/context-struct.md
- .agents/skills/patterns/factory.md
- .agents/skills/patterns/arena-allocator.md

## Pre-read Docs
- docs/backlog/manual-top-level-agents.md (full design spec)
- docs/backlog/shared-context-di.md (prerequisite design)
- docs/memory.md (talloc ownership)
- docs/rel-05/user-stories/01-spawn-agent.md
- docs/rel-05/user-stories/13-max-agents-limit.md

## Pre-read Source (patterns)
- src/repl.h (current ik_repl_ctx_t structure)
- src/repl_init.c (current initialization flow)
- src/agent.h (ik_agent_ctx_t from DI-1)
- src/agent.c (agent creation from DI-1)
- src/shared.h (ik_shared_ctx_t from DI-0)
- src/array.h (dynamic array pattern)

## Pre-read Tests (patterns)
- tests/unit/repl/repl_test.c (existing repl tests)
- tests/unit/agent/agent_test.c (agent tests from DI-1)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- DI-0 complete: `ik_shared_ctx_t` exists with all shared fields
- DI-1 complete: `ik_agent_ctx_t` exists with all per-agent fields
- `ik_repl_ctx_t` has single `ik_agent_ctx_t *agent` pointer

## Task
Convert `ik_repl_ctx_t` from single agent pointer to dynamic agents array. This is the foundational change enabling multi-agent support.

**Changes to `ik_repl_ctx_t`:**

```c
typedef struct ik_repl_ctx {
    // Shared infrastructure (from DI-0)
    ik_shared_ctx_t *shared;

    // Input parser (stateless, shared)
    ik_input_parser_t *input_parser;

    // Agent management (NEW)
    ik_agent_ctx_t **agents;        // Dynamic array of agent pointers
    size_t agent_count;             // Current number of agents
    size_t agent_capacity;          // Allocated capacity
    size_t current_agent_idx;       // Index of visible agent
    size_t next_agent_serial;       // Next ID to assign (never reused)

    // Exit flag
    atomic_bool quit;
} ik_repl_ctx_t;
```

**Key behaviors:**
- Maximum 20 agents (`IK_MAX_AGENTS`)
- `next_agent_serial` starts at 0, increments for each spawn, never decreases
- Agent 0/ created during `ik_repl_init()`
- `current_agent_idx` starts at 0
- Accessing current agent: `repl->agents[repl->current_agent_idx]`

**Talloc hierarchy:**
```
root_ctx
  ├─> shared_ctx (infrastructure)
  └─> repl_ctx (coordinator)
           └─> agents[0] (agent 0/)
           └─> agents[1] (agent 1/, when spawned)
           └─> ...
```

## TDD Cycle

### Red
1. Update `src/repl.h`:
   - Add `IK_MAX_AGENTS` constant (20)
   - Replace `ik_agent_ctx_t *agent` with array fields
   - Add `next_agent_serial` field

2. Create/update tests in `tests/unit/repl/repl_agent_array_test.c`:
   - Test repl_init creates agents array with capacity >= 1
   - Test repl_init creates agent 0/ as first element
   - Test `repl->agent_count == 1` after init
   - Test `repl->current_agent_idx == 0` after init
   - Test `repl->next_agent_serial == 1` after init (0 was used)
   - Test `repl->agents[0]->agent_id` equals "0/"
   - Test `repl->agents[0]->numeric_id` equals 0

3. Run `make check` - expect test failures

### Green
1. Update `src/repl_init.c`:
   ```c
   // Allocate agents array
   repl->agent_capacity = IK_MAX_AGENTS;
   repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, repl->agent_capacity);
   if (repl->agents == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

   // Create initial agent (agent 0/)
   ik_agent_ctx_t *agent0 = NULL;
   res_t res = ik_agent_create(repl, repl->next_agent_serial, &agent0);
   if (is_err(&res)) return res;

   repl->agents[0] = agent0;
   repl->agent_count = 1;
   repl->current_agent_idx = 0;
   repl->next_agent_serial = 1;
   ```

2. Update all code that accesses `repl->agent` to use `repl->agents[repl->current_agent_idx]`:
   - Create helper macro or inline: `#define CURRENT_AGENT(repl) ((repl)->agents[(repl)->current_agent_idx])`
   - Update repl_actions.c
   - Update repl_callbacks.c
   - Update repl_event_handlers.c
   - Update repl_tool.c
   - Update commands.c
   - Update event_render.c

3. Update cleanup to free all agents (talloc handles this via hierarchy)

4. Run `make check` - expect pass

### Refactor
1. Verify talloc ownership: agents array is child of repl_ctx
2. Verify each agent is child of repl_ctx (not child of array)
3. Verify CURRENT_AGENT macro is used consistently
4. Run `make lint` - verify clean
5. Consider: should agent_capacity be fixed at 20 or grow dynamically?
   - Decision: Fixed at 20 (simpler, matches max limit)

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- `ik_repl_ctx_t` has agents array instead of single agent pointer
- Agent 0/ automatically created on startup
- `next_agent_serial` tracks ID generation
- All existing functionality works unchanged (single agent behavior preserved)
- `IK_MAX_AGENTS` constant defined as 20
