# Task: Define ik_agent_display_t Struct

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 1 (Create Sub-Context Structs)

## Model

sonnet/thinking (11 fields, layers array)

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

- src/agent.h lines 68-82, 105-109 (display fields to extract)
- src/layer.h (layer types and cake)
- src/scrollback.h (scrollback type)
- src/agent.c (current display initialization in ik_agent_create)

### Test Patterns

- tests/unit/agent/agent_test.c (agent creation tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_identity_t` exists (from agent-identity-struct.md)
- No `ik_agent_display_t` struct exists yet

## Task

Create the `ik_agent_display_t` struct and factory function. This sub-context extracts 11 display-related fields from ik_agent_ctx_t.

### What

Create new header `src/agent_display.h` with:

```c
#pragma once

#include "layer.h"
#include "scrollback.h"

#include <talloc.h>
#include <stdbool.h>

// Layer indices for the layers array
typedef enum {
    IK_LAYER_SCROLLBACK = 0,
    IK_LAYER_SPINNER = 1,
    IK_LAYER_SEPARATOR = 2,
    IK_LAYER_INPUT = 3,
    IK_LAYER_COMPLETION = 4,
    IK_LAYER_COUNT = 5
} ik_layer_index_t;

// Agent display sub-context
//
// Contains all display/rendering state for an agent:
// - scrollback: conversation history display buffer
// - layer_cake: layer composition manager
// - layers: array of 5 layers (scrollback, spinner, separator, input, completion)
// - viewport_offset: scroll position in scrollback
// - spinner: spinner animation state
// - separator_visible: whether separator line is shown
// - input_buffer_visible: whether input is shown
// - input_text: current input text (borrowed from input_buffer)
// - input_text_len: length of input_text
//
// Ownership: talloc child of ik_agent_ctx_t
// Layer references are borrowed from layer_cake.
typedef struct {
    ik_scrollback_t *scrollback;
    ik_layer_cake_t *layer_cake;
    ik_layer_t *layers[IK_LAYER_COUNT];

    size_t viewport_offset;
    ik_spinner_state_t spinner;

    bool separator_visible;
    bool input_buffer_visible;

    // Input text reference (updated before render)
    const char *input_text;
    size_t input_text_len;
} ik_agent_display_t;

// Forward declaration
typedef struct ik_shared_ctx ik_shared_ctx_t;

// Create display sub-context
//
// Allocates display struct and creates scrollback buffer.
// Layer cake and layers are created during layer setup.
//
// @param ctx Talloc parent (agent context)
// @param shared Shared context for terminal width
// @param out Receives allocated display struct
// @return OK on success, ERR on failure
res_t ik_agent_display_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                               ik_agent_display_t **out);

// Convenience accessors for individual layers
static inline ik_layer_t *ik_agent_display_scrollback_layer(ik_agent_display_t *d) {
    return d->layers[IK_LAYER_SCROLLBACK];
}

static inline ik_layer_t *ik_agent_display_spinner_layer(ik_agent_display_t *d) {
    return d->layers[IK_LAYER_SPINNER];
}

static inline ik_layer_t *ik_agent_display_separator_layer(ik_agent_display_t *d) {
    return d->layers[IK_LAYER_SEPARATOR];
}

static inline ik_layer_t *ik_agent_display_input_layer(ik_agent_display_t *d) {
    return d->layers[IK_LAYER_INPUT];
}

static inline ik_layer_t *ik_agent_display_completion_layer(ik_agent_display_t *d) {
    return d->layers[IK_LAYER_COMPLETION];
}
```

### How

1. Create `src/agent_display.h` with struct and factory declaration
2. Create `src/agent_display.c` with factory implementation:

```c
#include "agent_display.h"
#include "error.h"
#include "panic.h"
#include "shared.h"
#include "terminal.h"

#include <assert.h>

res_t ik_agent_display_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                               ik_agent_display_t **out)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(shared != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);    // LCOV_EXCL_BR_LINE

    ik_agent_display_t *display = talloc_zero(ctx, ik_agent_display_t);
    if (display == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create scrollback buffer
    int32_t terminal_width = ik_term_width(shared->term);
    display->scrollback = ik_scrollback_create(display, terminal_width);
    if (display->scrollback == NULL) {
        talloc_free(display);
        return ERR("Failed to create scrollback buffer");
    }

    // Initialize display state
    display->viewport_offset = 0;
    display->spinner = IK_SPINNER_HIDDEN;
    display->separator_visible = false;
    display->input_buffer_visible = true;
    display->input_text = NULL;
    display->input_text_len = 0;

    // Layer array initialized to NULL - filled during layer setup
    for (int i = 0; i < IK_LAYER_COUNT; i++) {
        display->layers[i] = NULL;
    }

    *out = display;
    return OK(NULL);
}
```

3. Update Makefile to compile agent_display.c

### Why

The display sub-context has 11 fields managing visual rendering. Key design decisions:

- **Array for layers:** Using `layers[IK_LAYER_COUNT]` instead of 5 individual pointers enables iteration and reduces field count
- **Enum for indices:** Type-safe layer access via `IK_LAYER_SCROLLBACK` etc.
- **Inline accessors:** Maintain old-style `scrollback_layer` access during migration
- **res_t return:** Scrollback creation can fail (unlike identity which only PANICs)

**Note:** Layer creation happens in ik_agent_create, not here. This task creates the container; wiring happens in Phase 2.

## TDD Cycle

### Red

Create `tests/unit/agent/agent_display_test.c`:

```c
#include "agent_display.h"
#include "shared.h"
#include "test_utils.h"
#include <check.h>
#include <talloc.h>

static ik_shared_ctx_t *shared;
static TALLOC_CTX *test_ctx;

static void setup(void) {
    test_ctx = talloc_new(NULL);
    // Create minimal shared context with mock terminal
    shared = create_test_shared_ctx(test_ctx);
}

static void teardown(void) {
    talloc_free(test_ctx);
}

START_TEST(test_agent_display_create_succeeds) {
    ik_agent_display_t *display = NULL;
    res_t res = ik_agent_display_create(test_ctx, shared, &display);
    ck_assert(is_ok(res));
    ck_assert_ptr_nonnull(display);
}
END_TEST

START_TEST(test_agent_display_scrollback_created) {
    ik_agent_display_t *display = NULL;
    res_t res = ik_agent_display_create(test_ctx, shared, &display);
    ck_assert(is_ok(res));
    ck_assert_ptr_nonnull(display->scrollback);
}
END_TEST

START_TEST(test_agent_display_fields_init_correctly) {
    ik_agent_display_t *display = NULL;
    ik_agent_display_create(test_ctx, shared, &display);

    ck_assert_int_eq(display->viewport_offset, 0);
    ck_assert_int_eq(display->spinner, IK_SPINNER_HIDDEN);
    ck_assert(display->separator_visible == false);
    ck_assert(display->input_buffer_visible == true);
    ck_assert_ptr_null(display->input_text);
    ck_assert_int_eq(display->input_text_len, 0);
}
END_TEST

START_TEST(test_agent_display_layers_init_null) {
    ik_agent_display_t *display = NULL;
    ik_agent_display_create(test_ctx, shared, &display);

    for (int i = 0; i < IK_LAYER_COUNT; i++) {
        ck_assert_ptr_null(display->layers[i]);
    }
}
END_TEST

START_TEST(test_agent_display_layer_accessors) {
    ik_agent_display_t *display = NULL;
    ik_agent_display_create(test_ctx, shared, &display);

    // Accessors return NULL before layers are set up
    ck_assert_ptr_null(ik_agent_display_scrollback_layer(display));
    ck_assert_ptr_null(ik_agent_display_spinner_layer(display));
    ck_assert_ptr_null(ik_agent_display_separator_layer(display));
    ck_assert_ptr_null(ik_agent_display_input_layer(display));
    ck_assert_ptr_null(ik_agent_display_completion_layer(display));
}
END_TEST
```

Run `make check` - expect test failures.

### Green

1. Create `src/agent_display.h`
2. Create `src/agent_display.c`
3. Add to Makefile
4. Run `make check` - all tests pass

### Verify

- `make check` passes
- `make lint` passes
- Layer enum values are sequential (0-4)

## Post-conditions

- `src/agent_display.h` exists with struct definition
- `src/agent_display.c` exists with factory implementation
- `ik_agent_display_create()` works correctly
- Tests pass for display creation
- Scrollback is created as child of display
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If issues arise:
1. Remove `src/agent_display.h` and `src/agent_display.c`
2. Remove test file
3. Revert Makefile changes
4. `make check` should pass (back to original state)

## Sub-agent Usage

- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits
