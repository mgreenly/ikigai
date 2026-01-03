# Task: Add Display Sub-Context to ik_agent_ctx_t

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 2 (Composition)

## Model

sonnet/extended (many display fields)

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

- src/agent_display.h (display struct from Phase 1)
- src/agent.h (current ik_agent_ctx_t)
- src/agent.c (ik_agent_create, layer setup code)
- src/layer.h (layer types)

### Test Patterns

- tests/unit/agent/agent_test.c (agent creation tests)
- tests/unit/agent/agent_display_test.c (display tests from Phase 1)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_display_t` struct exists (from agent-display-struct.md)
- `ik_agent_identity_t *identity` field exists (from agent-compose-identity.md)
- No `display` field in ik_agent_ctx_t yet

## Task

Add the `ik_agent_display_t *display` field to `ik_agent_ctx_t` and create it during agent initialization. The old display fields remain for now - this is additive only.

### What

1. Add `#include "agent_display.h"` to agent.h
2. Add `ik_agent_display_t *display;` field to ik_agent_ctx_t
3. Create display sub-context in ik_agent_create()
4. Populate display fields from the corresponding agent fields
5. Wire up layer references to display->layers[] array

### How

**In `src/agent.h`:**

```c
#include "agent_display.h"  // Add after existing includes

typedef struct ik_agent_ctx {
    // ... identity fields ...

    // NEW: Focused sub-contexts
    ik_agent_identity_t *identity;
    ik_agent_display_t *display;

    // Display state (per-agent) - LEGACY, use display->* after migration
    ik_scrollback_t *scrollback;
    ik_layer_cake_t *layer_cake;
    ik_layer_t *scrollback_layer;
    // ... rest of display fields marked LEGACY ...

    // ... rest unchanged ...
} ik_agent_ctx_t;
```

**In `src/agent.c` (ik_agent_create):**

```c
res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                      const char *parent_uuid, ik_agent_ctx_t **out)
{
    // ... existing allocation and identity code ...

    // Create display sub-context
    res_t display_res = ik_agent_display_create(agent, shared, &agent->display);
    if (!is_ok(display_res)) {
        talloc_free(agent);
        return display_res;
    }

    // Legacy scrollback reference (for backward compatibility)
    agent->scrollback = agent->display->scrollback;

    // ... existing layer_cake creation ...
    agent->layer_cake = ik_layer_cake_create(agent);
    agent->display->layer_cake = agent->layer_cake;

    // ... existing layer creation ...
    // After creating each layer, populate both legacy and display array:

    // Scrollback layer
    agent->scrollback_layer = ik_layer_create(/* ... */);
    agent->display->layers[IK_LAYER_SCROLLBACK] = agent->scrollback_layer;

    // Spinner layer
    agent->spinner_layer = ik_layer_create(/* ... */);
    agent->display->layers[IK_LAYER_SPINNER] = agent->spinner_layer;

    // Separator layer
    agent->separator_layer = ik_layer_create(/* ... */);
    agent->display->layers[IK_LAYER_SEPARATOR] = agent->separator_layer;

    // Input layer
    agent->input_layer = ik_layer_create(/* ... */);
    agent->display->layers[IK_LAYER_INPUT] = agent->input_layer;

    // Completion layer
    agent->completion_layer = ik_layer_create(/* ... */);
    agent->display->layers[IK_LAYER_COMPLETION] = agent->completion_layer;

    // Synchronize display state fields
    agent->display->viewport_offset = agent->viewport_offset;
    agent->display->spinner = agent->spinner_state;
    agent->display->separator_visible = agent->separator_visible;
    agent->display->input_buffer_visible = agent->input_buffer_visible;

    // ... rest unchanged ...
}
```

### Why

The display sub-context has 11 fields - the largest group. Key considerations:

1. **Scrollback shared:** `display->scrollback` is the authoritative owner; legacy field is a reference
2. **Layer array:** Layers stored in array for iteration, legacy individual pointers remain
3. **Layer cake shared:** Both legacy and display point to same layer_cake
4. **State fields synchronized:** viewport_offset, spinner, visibility flags

The pattern enables gradual migration from `agent->scrollback_layer` to `agent->display->layers[IK_LAYER_SCROLLBACK]`.

## TDD Cycle

### Red

Update `tests/unit/agent/agent_test.c`:

```c
START_TEST(test_agent_create_has_display) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // New: verify display sub-context exists
    ck_assert_ptr_nonnull(agent->display);
}
END_TEST

START_TEST(test_agent_display_scrollback_matches_legacy) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // Verify scrollback is shared
    ck_assert_ptr_eq(agent->display->scrollback, agent->scrollback);
}
END_TEST

START_TEST(test_agent_display_layers_match_legacy) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // Verify layer array matches individual pointers
    ck_assert_ptr_eq(agent->display->layers[IK_LAYER_SCROLLBACK], agent->scrollback_layer);
    ck_assert_ptr_eq(agent->display->layers[IK_LAYER_SPINNER], agent->spinner_layer);
    ck_assert_ptr_eq(agent->display->layers[IK_LAYER_SEPARATOR], agent->separator_layer);
    ck_assert_ptr_eq(agent->display->layers[IK_LAYER_INPUT], agent->input_layer);
    ck_assert_ptr_eq(agent->display->layers[IK_LAYER_COMPLETION], agent->completion_layer);
}
END_TEST

START_TEST(test_agent_display_state_matches_legacy) {
    // ... setup ...
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(res));

    // Verify state fields match
    ck_assert_int_eq(agent->display->viewport_offset, agent->viewport_offset);
    ck_assert(agent->display->input_buffer_visible == agent->input_buffer_visible);
}
END_TEST
```

Run `make check` - expect failures.

### Green

1. Add include and field to `src/agent.h`
2. Update `ik_agent_create()` to create and populate display
3. Wire layer references to display->layers[] array
4. Synchronize state fields
5. Run `make check` - all tests pass

### Verify

- `make check` passes
- `make lint` passes
- Existing agent tests still pass
- Display layer references match legacy pointers
- Rendering still works (manual verification)

## Post-conditions

- `ik_agent_ctx_t` has `display` field
- `ik_agent_create()` creates display sub-context
- Display scrollback, layer_cake, and layers populated
- Display state fields synchronized
- All existing tests pass unchanged
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If issues arise:
1. Remove `display` field from ik_agent_ctx_t
2. Remove display creation from ik_agent_create
3. Remove include
4. Remove new tests
5. `make check` should pass (back to original state)

## Sub-agent Usage

- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits
