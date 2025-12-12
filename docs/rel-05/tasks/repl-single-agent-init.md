# Task: Initialize Single Agent at Startup

## Target
Phase 0: Agent Context Integration - Step 2 (create and wire up single agent)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/scm.md

## Pre-read Docs
- docs/rel-05/README.md (architecture overview)
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/agent.h (agent context)
- src/agent.c (agent creation)
- src/repl.h (repl context with current pointer)
- src/repl_init.c (initialization flow)

## Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c
- tests/unit/repl/*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_repl_ctx_t` has `current` field
- All agent-ctx-*.md tasks complete (agent owns all per-agent state)

## Task
Create a single agent at REPL startup and set it as the current agent. This completes Phase 0 - the system works exactly as before, but the architecture supports multiple agents.

After this task:
- `repl->current` points to the single agent
- All per-agent state accessed via `repl->current->*`
- Application behavior unchanged from user perspective

## TDD Cycle

### Red
1. Update `tests/unit/repl/repl_init_test.c` (or create if needed):
   - Test `repl->current` is non-NULL after init
   - Test `repl->current->uuid` is valid
   - Test `repl->current->parent_uuid` is NULL (root agent)
   - Test `repl->current->shared` equals `repl->shared`

2. Run `make check` - expect failures

### Green
1. Update `src/repl_init.c`:
   - Add include for agent.h
   - After shared context setup, create the root agent:
     ```c
     ik_agent_ctx_t *agent = NULL;
     res_t result = ik_agent_create(repl, repl->shared, NULL, &agent);
     if (IS_ERR(result)) {
         // Handle error appropriately
         return result;
     }
     repl->current = agent;
     ```

2. Verify all code paths that access per-agent state now go through `repl->current`

3. Run `make check` - expect pass

### Refactor
1. Verify talloc hierarchy:
   ```
   root_ctx
     |-> shared_ctx
     +-> repl_ctx
              +-> agent_ctx (repl->current)
   ```

2. Verify cleanup path frees agent when repl is freed (automatic via talloc)

3. Run `make lint` - verify clean

4. Manual smoke test: run ikigai, verify it works as before

## Post-conditions
- `make check` passes
- `make lint` passes
- Single agent created at startup with UUID identity
- `repl->current` points to this agent
- All per-agent state accessed via `repl->current->*`
- Application behavior unchanged from user perspective
- Working tree is clean (all changes committed)

## Verification
After this task, Phase 0 is complete. Verify:

1. **Architecture**: Code is structured for multi-agent support
2. **Functionality**: Single agent works exactly as before
3. **Identity**: Agent has UUID (visible in debug if needed)
4. **Ownership**: talloc hierarchy is correct

The system is now ready for Phase 1 (registry) and Phase 2 (multiple agents).
