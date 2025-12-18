# Task: Migrate Identity Field Callers

## Target

Refactoring #1: Decompose `ik_agent_ctx_t` God Object - Identity Field Migration

## Pre-read Skills

- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Source (patterns)

- src/agent.h (ik_agent_ctx_t with embedded ik_agent_identity_t)
- src/agent.c (ik_agent_create, ik_agent_restore)

## Pre-read Tests (patterns)

- tests/unit/agent/agent_test.c (updated accessor patterns)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Task `agent-compose-subcontexts` is complete
- `ik_agent_ctx_t` embeds `ik_agent_identity_t identity`
- Tests already updated to use `agent->identity.uuid` etc.

## Task

Migrate all production code (src/*.c) to use the new identity field accessor pattern.

### What

Update all source files that access identity fields to use the new path:

| Old Pattern | New Pattern |
|-------------|-------------|
| `agent->uuid` | `agent->identity.uuid` |
| `agent->name` | `agent->identity.name` |
| `agent->parent_uuid` | `agent->identity.parent_uuid` |
| `agent->created_at` | `agent->identity.created_at` |
| `agent->fork_message_id` | `agent->identity.fork_message_id` |

### How

1. **Discovery Phase** (use sub-agents):

   Search BOTH access patterns - direct agent and via repl->current:
   ```bash
   # Direct agent access
   grep -rn "agent->uuid" src/
   grep -rn "agent->name" src/
   grep -rn "agent->parent_uuid" src/
   grep -rn "agent->created_at" src/
   grep -rn "agent->fork_message_id" src/

   # Indirect via repl->current (accounts for 80%+ of callsites)
   grep -rn "repl->current->uuid" src/
   grep -rn "repl->current->name" src/
   grep -rn "repl->current->parent_uuid" src/
   grep -rn "repl->current->created_at" src/
   grep -rn "repl->current->fork_message_id" src/
   ```

2. **Update each file**:
   - For each file with matches, update the accessor pattern
   - Preserve surrounding code exactly
   - Run `make check` after each file to catch errors early

3. **Expected files** (based on grep patterns):
   - `src/agent.c` - Factory functions (already updated in compose task)
   - `src/db/agent.c` - Database operations
   - `src/commands_agent.c` - /agents command
   - `src/commands_fork.c` - /fork command
   - `src/repl_init.c` - Agent registration
   - `src/layer_wrappers.c` - Separator layer (shows agent name)

### Why

After the compose task, tests use new accessors but production code still uses old patterns. This migration task:
- Updates production code to match test patterns
- Removes compiler warnings/errors about missing fields
- Completes the identity extraction

## TDD Cycle

### Red

The code may already fail to compile after compose task if the old field paths no longer exist in `ik_agent_ctx_t`.

If it does compile (due to compatibility layer), tests should still pass since they were already updated.

### Green

1. Run grep to find all callers
2. Update each source file
3. Run `make check` after each file
4. Continue until all files updated

### Refactor

1. Verify no old patterns remain: `grep -rn "agent->uuid\b" src/` should return empty (or only show new pattern contexts)
2. Run `make lint` - verify clean

## Post-conditions

- `make check` passes
- `make lint` passes
- No source files use `agent->uuid` (use `agent->identity.uuid`)
- No source files use `agent->name` (use `agent->identity.name`)
- No source files use `agent->parent_uuid` (use `agent->identity.parent_uuid`)
- No source files use `agent->created_at` (use `agent->identity.created_at`)
- No source files use `agent->fork_message_id` (use `agent->identity.fork_message_id`)
- Working tree is clean (all changes committed)

## Sub-agent Usage

**RECOMMENDED: Use sub-agents for this task**

Identity fields have relatively few usages (estimated 20-30 locations). Sub-agents can:
1. Run grep to identify all locations
2. Process files in parallel batches
3. Verify each batch with `make check`

Pattern for sub-agent:
```
Search for all "agent->uuid" usages in src/
For each file:
  - Read the file
  - Replace "agent->uuid" with "agent->identity.uuid"
  - Write the file
  - Run make check
```

## Notes

Be careful with:
- `agent->name` vs other uses of `->name` (only change agent context accesses)
- String formatting that includes `agent->uuid` - ensure format strings still work
- Null checks on identity fields should still work the same way

The identity fields are the simplest to migrate because they are:
- Read-only after creation
- Simple string/integer types
- Not passed by reference to other functions
