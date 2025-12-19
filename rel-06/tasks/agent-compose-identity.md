# Task: Add Identity Sub-Context to ik_agent_ctx_t

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 2 (Composition)

## Model

sonnet/thinking (modify agent creation)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm
- patterns/factory

### Docs

- project/memory.md (talloc ownership patterns)
- rel-06/refactor.md (Section 1: Decompose ik_agent_ctx_t)

### Source Files

- src/agent_identity.h (identity struct from Phase 1)
- src/agent.h (current ik_agent_ctx_t)
- src/agent.c (ik_agent_create, ik_agent_restore)

### Test Patterns

- tests/unit/agent/agent_test.c (agent creation tests)
- tests/unit/agent/agent_identity_test.c (identity tests from Phase 1)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_identity_t` struct exists (from agent-identity-struct.md)
- No `identity` field in ik_agent_ctx_t yet

## Task

Add the `ik_agent_identity_t *identity` field to `ik_agent_ctx_t` and create it during agent initialization. The old fields remain for now - this is additive only.

### What

1. Add `#include "agent_identity.h"` to agent.h
2. Add `ik_agent_identity_t *identity;` field to ik_agent_ctx_t
3. Create identity sub-context in ik_agent_create()
4. Create identity sub-context in ik_agent_restore()
5. Populate identity fields from the corresponding agent fields

### How

**In `src/agent.h`:**

```c
#include "agent_identity.h"  // Add after existing includes

typedef struct ik_agent_ctx {
    // Identity (from agent-process-model.md)
    char *uuid;          // LEGACY - use identity->uuid after migration
    char *name;          // LEGACY - use identity->name after migration
    char *parent_uuid;   // LEGACY - use identity->parent_uuid after migration
    int64_t created_at;       // LEGACY - use identity->created_at after migration
    int64_t fork_message_id;  // LEGACY - use identity->fork_message_id after migration

    // NEW: Focused sub-contexts
    ik_agent_identity_t *identity;

    // ... rest unchanged ...
} ik_agent_ctx_t;
```

**In `src/agent.c` (ik_agent_create):**

```c
res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                      const char *parent_uuid, ik_agent_ctx_t **out)
{
    // ... existing allocation code ...

    // Create identity sub-context
    agent->identity = ik_agent_identity_create(agent);

    // Generate UUID (sets both legacy field AND identity field)
    agent->uuid = ik_generate_uuid(agent);
    agent->identity->uuid = talloc_reference(agent->identity, agent->uuid);

    // Set timestamps
    agent->created_at = time(NULL);
    agent->identity->created_at = agent->created_at;

    // Set parent UUID if forking
    if (parent_uuid != NULL) {
        agent->parent_uuid = talloc_strdup(agent, parent_uuid);
        agent->identity->parent_uuid = talloc_reference(agent->identity, agent->parent_uuid);
    }

    // fork_message_id set by caller after creation
    // identity->fork_message_id also needs to be set by caller

    // ... rest unchanged ...
}
```

**In `src/agent.c` (ik_agent_restore):**

```c
res_t ik_agent_restore(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                        const void *row_ptr, ik_agent_ctx_t **out)
{
    // ... existing allocation code ...

    // Create identity sub-context
    agent->identity = ik_agent_identity_create(agent);

    // Copy from DB row (sets both legacy field AND identity field)
    agent->uuid = talloc_strdup(agent, row->uuid);
    agent->identity->uuid = talloc_reference(agent->identity, agent->uuid);

    agent->created_at = row->created_at;
    agent->identity->created_at = row->created_at;

    agent->fork_message_id = row->fork_message_id;
    agent->identity->fork_message_id = row->fork_message_id;

    if (row->name != NULL) {
        agent->name = talloc_strdup(agent, row->name);
        agent->identity->name = talloc_reference(agent->identity, agent->name);
    }

    if (row->parent_uuid != NULL) {
        agent->parent_uuid = talloc_strdup(agent, row->parent_uuid);
        agent->identity->parent_uuid = talloc_reference(agent->identity, agent->parent_uuid);
    }

    // ... rest unchanged ...
}
```

### Why

This task establishes the composition pattern:

1. **Additive change:** Old fields remain - existing code continues to work
2. **Dual population:** Both legacy and new fields populated during creation
3. **talloc_reference:** Strings shared between legacy and new locations without duplication
4. **Zero migration risk:** No callers need to change yet

The pattern enables incremental migration: callers can switch from `agent->uuid` to `agent->identity->uuid` one at a time.

## TDD Cycle

### Red

Update `tests/unit/agent/agent_test.c`:

```c
START_TEST(test_agent_create_has_identity) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // New: verify identity sub-context exists
    ck_assert_ptr_nonnull(agent->identity);
}
END_TEST

START_TEST(test_agent_identity_matches_legacy_fields) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // Verify identity fields match legacy fields
    ck_assert_str_eq(agent->identity->uuid, agent->uuid);
    ck_assert_int_eq(agent->identity->created_at, agent->created_at);
}
END_TEST

START_TEST(test_agent_restore_has_identity) {
    // ... setup with mock DB row ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(test_ctx, shared, &row, &agent);
    ck_assert(is_ok(res));

    ck_assert_ptr_nonnull(agent->identity);
    ck_assert_str_eq(agent->identity->uuid, row.uuid);
}
END_TEST
```

Run `make check` - expect failures (identity field doesn't exist).

### Green

1. Add include and field to `src/agent.h`
2. Update `ik_agent_create()` to create and populate identity
3. Update `ik_agent_restore()` to create and populate identity
4. Run `make check` - all tests pass

### Verify

- `make check` passes
- `make lint` passes
- Existing agent tests still pass
- New identity tests pass
- `agent->identity->uuid` equals `agent->uuid`

## Post-conditions

- `ik_agent_ctx_t` has `identity` field
- `ik_agent_create()` creates identity sub-context
- `ik_agent_restore()` creates identity sub-context
- Identity fields populated from legacy fields
- All existing tests pass unchanged
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If issues arise:
1. Remove `identity` field from ik_agent_ctx_t
2. Remove identity creation from ik_agent_create/restore
3. Remove include
4. Remove new tests
5. `make check` should pass (back to original state)

## Sub-agent Usage

- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits
