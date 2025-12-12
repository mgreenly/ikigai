# Task: Add Agent Pointer to REPL Context

## Target
Phase 0: Agent Context Integration - Step 1 (add current agent pointer)

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
- src/agent.h (agent context structure)
- src/repl.h (repl context to modify)
- src/shared.h (shared context pattern)

## Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c
- tests/unit/repl/*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_ctx_t` exists with identity fields
- Agent context extraction complete (all agent-ctx-*.md tasks done)

## Task
Add a `current` agent pointer to `ik_repl_ctx_t`. This is the integration point between the coordinator (repl) and the per-agent state.

For Phase 0, this is a simple pointer to a single agent. In later phases, this becomes the "current" pointer into an agent array/registry.

## TDD Cycle

### Red
1. Update `src/repl.h`:
   - Add forward declaration (if not already present):
     ```c
     typedef struct ik_agent_ctx ik_agent_ctx_t;
     ```
   - Add field to `ik_repl_ctx_t`:
     ```c
     // Current agent (per-agent state)
     // In Phase 0: single agent
     // In later phases: current selection from agent array
     ik_agent_ctx_t *current;
     ```

2. Update tests to verify `repl->current` exists and can be accessed

3. Run `make check` - expect failures (field not initialized)

### Green
1. The field is added but not yet initialized - that comes in the next task (repl-single-agent-init.md)

2. For now, ensure the field is zero-initialized (via talloc_zero) so `repl->current` is NULL until explicitly set

3. Run `make check` - expect pass (NULL is acceptable for now)

### Refactor
1. Verify the forward declaration doesn't create circular includes
2. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `make lint` passes
- `ik_repl_ctx_t` has `current` field of type `ik_agent_ctx_t *`
- Field is NULL after repl creation (until initialized)
- Working tree is clean (all changes committed)
