# Task: Fix Missing Lower Separator in Sub-agents

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided below.

**Model:** sonnet/thinking
**Depends on:** None

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Pre-Read

**Skills:**
- `/load errors` - Result type patterns (is_err, PANIC for allocation failure)
- `/load style` - Code style conventions

**Source:**
- `src/commands_fork.c` - Fork command implementation (modify after agent creation)
- `src/repl/agent_restore.c` - Agent restoration (modify after agent restore)
- `src/repl_init.c` - Reference: see how lower_separator_layer is added to Agent 0's layer cake (line ~98)
- `src/layer.h` - Reference: `ik_layer_cake_add_layer()` signature

**Plan:**
- `cdd/plan/separator-missing.md` - Full analysis

## Libraries

Use only existing project libraries. Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)

## Objective

Add the shared `repl->lower_separator_layer` to child agent layer cakes so the lower separator renders for all agents, not just Agent 0.

**Problem:** The lower separator is created in `repl_init.c` and added only to Agent 0's layer cake. Child agents created via `/fork` or restored from database don't have it.

**Solution:** Add `repl->lower_separator_layer` to each agent's layer cake after creation/restoration.

## Changes Required

### src/commands_fork.c

Find the location after `ik_agent_create()` succeeds and `child->repl = repl` is set (approximately line 202).

Add after `child->repl = repl;`:

```c
// Add shared lower separator to child's layer cake
res = ik_layer_cake_add_layer(child->layer_cake, repl->lower_separator_layer);
if (is_err(&res)) PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
```

### src/repl/agent_restore.c

Find the `restore_child_agent()` function. After `agent->repl = repl` is set (approximately line 157).

Add after `agent->repl = repl;`:

```c
// Add shared lower separator to restored agent's layer cake
res = ik_layer_cake_add_layer(agent->layer_cake, repl->lower_separator_layer);
if (is_err(&res)) PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
```

**Key points:**
- Use `res` variable (should already exist in scope from prior operations)
- PANIC on allocation failure with `LCOV_EXCL_BR_LINE` marker
- The layer is shared (owned by repl), not created new for each agent
- Follow existing code patterns in these files

## Layer Cake After Fix

All agents will have the same layer stack:
1. scrollback_layer
2. spinner_layer
3. separator_layer (upper)
4. input_layer
5. completion_layer
6. lower_separator_layer (shared from repl)

## Test Scenarios

Run `make check` to verify no regressions. The existing test infrastructure should pass.

Manual verification (if available):
1. `/fork test` - create child agent
2. Switch to child agent
3. Verify lower separator is visible

## Completion

After completing work (whether success, partial, or failed), commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(separator-missing.md): success - add lower separator to child agent layer cakes

Added repl->lower_separator_layer to child agent layer cakes in both
commands_fork.c (new agents) and agent_restore.c (restored agents).
EOF
)"
```

Report status to orchestration:
- Success: `/task-done separator-missing.md`
- Partial/Failed: `/task-fail separator-missing.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
