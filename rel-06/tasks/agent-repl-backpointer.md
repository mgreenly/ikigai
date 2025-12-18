# Task: Add REPL Backpointer to Agent

## Target

Background Agent Event Loop (gap.md) - Foundation

## Pre-read

### Skills

- default
- tdd
- style
- errors
- quality
- scm

### Source Files

- src/agent.h (agent structure definition)
- src/agent.c (agent creation)
- src/repl_init.c (where agents are created and added to repl)
- src/repl/session_restore.c (where agents are restored from database)

### Test Patterns

- tests/unit/agent/agent_test.c (existing agent tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Task

Add a `repl` backpointer field to `ik_agent_ctx_t` so callbacks can access the REPL context from an agent pointer.

### What

Add `struct ik_repl_ctx_t *repl` field to the agent context structure. Set this field after agent creation in all code paths.

### How

1. In `src/agent.h`:
   - Add forward declaration: `struct ik_repl_ctx_t;`
   - Add field to `ik_agent_ctx_t`: `struct ik_repl_ctx_t *repl;`

2. In `src/repl_init.c`:
   - After `ik_agent_create()` returns, set `agent->repl = repl`
   - This happens before `ik_repl_add_agent()`

3. In `src/repl/session_restore.c`:
   - After `ik_agent_restore()` returns, set `agent->repl = repl`
   - Search for all `ik_agent_restore` calls and add the assignment

4. In `src/commands_fork.c`:
   - After creating child agent, set `child->repl = repl`

### Why

Callbacks currently receive `repl` as context and use `repl->current` to find the target agent. This breaks when user switches agents - callbacks update the wrong agent. By passing agent to callbacks (future task), callbacks need a way to access the REPL for operations like rendering. The backpointer provides this access.

## TDD Cycle

### Red

Write a test that:
1. Creates a REPL context with an agent
2. Verifies `agent->repl` equals the REPL context
3. Expect test to fail (field doesn't exist yet)

### Green

1. Add the field declaration to agent.h
2. Add the forward declaration for ik_repl_ctx_t
3. Set the backpointer in repl_init.c
4. Set the backpointer in session_restore.c
5. Set the backpointer in commands_fork.c
6. Run `make check` - test should pass

### Verify

- `make check` passes
- `make lint` passes

## Post-conditions

- `ik_agent_ctx_t` has `struct ik_repl_ctx_t *repl` field
- All agent creation paths set `agent->repl` to the parent REPL
- All existing tests still pass
- New test verifies backpointer is set correctly
