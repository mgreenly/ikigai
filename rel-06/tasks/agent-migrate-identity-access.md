# Task: Migrate Identity Field Access

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 3 (Migration)

## Model

sonnet/thinking (many files affected - use sub-agents)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm

### Docs

- project/memory.md (talloc ownership)
- rel-06/refactor.md (Section 1: Decompose ik_agent_ctx_t)

### Source Files

- src/agent_identity.h (identity struct)
- src/agent.h (current ik_agent_ctx_t with both legacy and identity fields)

### Test Patterns

- tests/unit/agent/agent_test.c (agent tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_identity_t *identity` field exists in ik_agent_ctx_t
- Both legacy and identity fields populated during creation

## Task

Migrate all access to identity fields from legacy (`agent->uuid`) to sub-context (`agent->identity->uuid`). This is a mechanical find/replace across all source files.

### What

Replace all occurrences:
- `agent->uuid` -> `agent->identity->uuid`
- `agent->name` -> `agent->identity->name`
- `agent->parent_uuid` -> `agent->identity->parent_uuid`
- `agent->created_at` -> `agent->identity->created_at`
- `agent->fork_message_id` -> `agent->identity->fork_message_id`

Also for variable names like `current->uuid`, `child->name`, etc.

### How

**Step 1: Identify all files with identity field access**

```bash
# Find all files accessing identity fields
grep -rn "->uuid\|->name\|->parent_uuid\|->created_at\|->fork_message_id" src/ tests/ \
    | grep -v "identity->" \
    | grep -v "row->" \
    | grep -v "config->" \
    | grep -v "msg->" \
    > /tmp/identity_access.txt
```

**Step 2: Categorize by pattern**

Common patterns to handle:
1. `agent->uuid` - direct agent access
2. `current->uuid` - via repl->current
3. `child->uuid` - local variable
4. `parent->uuid` - function parameter

**Step 3: Update each file**

For each source file, replace:

```c
// Before
const char *uuid = agent->uuid;
agent->uuid = ik_generate_uuid(agent);

// After
const char *uuid = agent->identity->uuid;
agent->identity->uuid = ik_generate_uuid(agent->identity);
```

**Step 4: Update agent.c (special case)**

In `ik_agent_create()` and `ik_agent_restore()`, only set `identity->` fields, not legacy fields:

```c
// Before (transition period)
agent->uuid = ik_generate_uuid(agent);
agent->identity->uuid = talloc_reference(agent->identity, agent->uuid);

// After (migration complete)
agent->identity->uuid = ik_generate_uuid(agent->identity);
// No legacy field assignment
```

### Why

This migration:
1. Moves field ownership to the identity sub-context
2. Removes string sharing between legacy and new locations
3. Enables removal of legacy fields in Phase 4
4. Must be done carefully to avoid breaking any callers

### Files Likely Affected

Based on codebase analysis, these files likely access identity fields:
- `src/agent.c` - creation, restore, copy
- `src/commands.c` - /fork, /kill commands
- `src/commands_agent.c` - /agents tree command
- `src/db/agent.c` - database operations
- `src/repl.c` - REPL initialization
- `src/repl_event_handlers.c` - event handling
- `tests/unit/agent/*.c` - agent tests
- `tests/integration/*.c` - integration tests

## TDD Cycle

### Red

No new tests needed - existing tests verify behavior. Run `make check` before migration to establish baseline.

### Green

**CRITICAL: Use sub-agents for parallel migration**

Spawn haiku sub-agents to migrate files in parallel:

1. **Sub-agent 1:** Migrate `src/agent.c`
2. **Sub-agent 2:** Migrate `src/commands.c` and `src/commands_agent.c`
3. **Sub-agent 3:** Migrate `src/db/agent.c`
4. **Sub-agent 4:** Migrate `src/repl*.c` files
5. **Sub-agent 5:** Migrate test files

Each sub-agent:
1. Opens assigned file(s)
2. Replaces `->uuid` with `->identity->uuid` (for agent variables only)
3. Replaces `->name` with `->identity->name` (for agent variables only)
4. Replaces `->parent_uuid` with `->identity->parent_uuid`
5. Replaces `->created_at` with `->identity->created_at`
6. Replaces `->fork_message_id` with `->identity->fork_message_id`
7. Runs `make check` to verify

**IMPORTANT:** Be careful to only replace agent struct access, not:
- `row->uuid` (database row)
- `config->name` (config struct)
- `msg->uuid` (message struct)

### Verify

After all sub-agents complete:
- `make check` passes
- `make lint` passes
- No remaining `agent->uuid` (except in comments/strings)
- `grep -rn "agent->uuid" src/ tests/` returns empty

## Post-conditions

- All identity field access uses `agent->identity->*`
- No code uses legacy `agent->uuid`, `agent->name`, etc.
- All existing tests pass
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If migration fails partway:
1. `git checkout -- .` to revert all changes
2. Debug failing test
3. Try migration again with fix
4. If repeated failures, reconsider approach

**Alternative:** Migrate one file at a time with commit after each, enabling partial rollback.

## Sub-agent Usage

**MANDATORY:** Use sub-agents for parallel file migration.

```
Sub-agent assignments:
- Agent A: src/agent.c (highest risk - create/restore)
- Agent B: src/commands.c, src/commands_agent.c
- Agent C: src/db/agent.c
- Agent D: src/repl.c, src/repl_event_handlers.c, src/repl_actions_llm.c
- Agent E: tests/unit/agent/*.c
- Agent F: tests/integration/*.c

Each agent runs `make check` after completing their files.
Main agent coordinates and runs final `make check`.
```

This approach:
1. Parallelizes mechanical work
2. Isolates failures to specific files
3. Enables incremental progress
4. Reduces total migration time
