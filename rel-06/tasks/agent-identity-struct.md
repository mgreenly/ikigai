# Task: Define ik_agent_identity_t Struct

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 1 (Create Sub-Context Structs)

## Model

sonnet (simple struct definition)

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

- src/agent.h lines 54-60 (identity fields to extract)
- src/agent.c (current ik_agent_create implementation)

### Test Patterns

- tests/unit/agent/agent_test.c (agent creation tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- No `ik_agent_identity_t` struct exists yet

## Task

Create the `ik_agent_identity_t` struct and factory function. This is the first sub-context struct in the agent decomposition, extracting 5 identity-related fields.

### What

Create new header `src/agent_identity.h` with:

```c
#pragma once

#include <talloc.h>
#include <stdint.h>

// Agent identity sub-context
//
// Contains immutable identity information for an agent:
// - uuid: unique identifier (base64url, 22 chars)
// - name: optional human-readable name
// - parent_uuid: parent agent's UUID (NULL for root)
// - created_at: Unix timestamp of creation
// - fork_message_id: message ID at fork point (0 for root)
//
// Ownership: talloc child of ik_agent_ctx_t
typedef struct {
    char *uuid;
    char *name;
    char *parent_uuid;
    int64_t created_at;
    int64_t fork_message_id;
} ik_agent_identity_t;

// Create identity sub-context
//
// Allocates and initializes identity struct. Does NOT generate UUID -
// caller must set uuid field after creation.
//
// @param ctx Talloc parent (agent context)
// @param out Receives allocated identity struct
// @return OK on success (out set), PANIC on OOM
ik_agent_identity_t *ik_agent_identity_create(TALLOC_CTX *ctx);
```

### How

1. Create `src/agent_identity.h` with struct and factory declaration
2. Create `src/agent_identity.c` with factory implementation:

```c
#include "agent_identity.h"
#include "panic.h"

#include <assert.h>

ik_agent_identity_t *ik_agent_identity_create(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    ik_agent_identity_t *identity = talloc_zero(ctx, ik_agent_identity_t);
    if (identity == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // All fields initialize to zero/NULL via talloc_zero
    // - uuid: NULL (caller sets via ik_generate_uuid or restore)
    // - name: NULL (optional)
    // - parent_uuid: NULL (set for forked agents)
    // - created_at: 0 (caller sets)
    // - fork_message_id: 0 (set for forked agents)

    return identity;
}
```

3. Update Makefile to compile agent_identity.c

### Why

This is the simplest sub-context (5 fields, no complex initialization). It establishes the pattern for other sub-contexts:

- Struct in dedicated header
- Factory returns direct pointer (only OOM can fail -> PANIC)
- talloc_zero for safe initialization
- Caller responsible for populating fields

**Note:** This task creates the struct only. Integration with ik_agent_ctx_t happens in Phase 2 (agent-compose-identity.md).

## TDD Cycle

### Red

Create `tests/unit/agent/agent_identity_test.c`:

```c
#include "agent_identity.h"
#include <check.h>
#include <talloc.h>

START_TEST(test_agent_identity_create_returns_non_null) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_identity_t *identity = ik_agent_identity_create(ctx);
    ck_assert_ptr_nonnull(identity);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_agent_identity_fields_init_zero) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_identity_t *identity = ik_agent_identity_create(ctx);

    ck_assert_ptr_null(identity->uuid);
    ck_assert_ptr_null(identity->name);
    ck_assert_ptr_null(identity->parent_uuid);
    ck_assert_int_eq(identity->created_at, 0);
    ck_assert_int_eq(identity->fork_message_id, 0);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_agent_identity_is_child_of_ctx) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_identity_t *identity = ik_agent_identity_create(ctx);

    // Verify talloc parent is ctx
    ck_assert_ptr_eq(talloc_parent(identity), ctx);

    talloc_free(ctx);  // Should free identity too
}
END_TEST

START_TEST(test_agent_identity_uuid_can_be_set) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_identity_t *identity = ik_agent_identity_create(ctx);

    identity->uuid = talloc_strdup(identity, "test-uuid-12345");
    ck_assert_str_eq(identity->uuid, "test-uuid-12345");

    talloc_free(ctx);
}
END_TEST
```

Run `make check` - expect test failures (header not found).

### Green

1. Create `src/agent_identity.h`
2. Create `src/agent_identity.c`
3. Add to Makefile
4. Run `make check` - all tests pass

### Verify

- `make check` passes
- `make lint` passes
- New files follow naming and style conventions

## Post-conditions

- `src/agent_identity.h` exists with struct definition
- `src/agent_identity.c` exists with factory implementation
- `ik_agent_identity_create()` works correctly
- Tests pass for identity creation
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If issues arise:
1. Remove `src/agent_identity.h` and `src/agent_identity.c`
2. Remove test file
3. Revert Makefile changes
4. `make check` should pass (back to original state)

## Sub-agent Usage

- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits
