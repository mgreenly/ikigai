# Task: Define ik_agent_llm_t Struct

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 1 (Create Sub-Context Structs)

## Model

sonnet/thinking (9 fields)

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

- src/agent.h lines 21-25, 94-103 (state enum and LLM fields to extract)
- src/openai/client_multi.h (multi handle type)
- src/agent.c (current LLM initialization in ik_agent_create)

### Test Patterns

- tests/unit/agent/agent_test.c (agent creation tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_identity_t` exists (from agent-identity-struct.md)
- `ik_agent_display_t` exists (from agent-display-struct.md)
- No `ik_agent_llm_t` struct exists yet

## Task

Create the `ik_agent_llm_t` struct and factory function. This sub-context extracts 9 LLM interaction fields from ik_agent_ctx_t.

### What

Create new header `src/agent_llm.h` with:

```c
#pragma once

#include <talloc.h>
#include <stdint.h>

// Forward declarations
struct ik_openai_multi;

// Agent LLM interaction state
//
// LLM state machine
typedef enum {
    IK_AGENT_STATE_IDLE,              // Normal input mode
    IK_AGENT_STATE_WAITING_FOR_LLM,   // Waiting for LLM response (spinner visible)
    IK_AGENT_STATE_EXECUTING_TOOL     // Tool running in background thread
} ik_agent_state_t;

// Agent LLM sub-context
//
// Contains all LLM interaction state for an agent:
// - multi: curl multi handle for async HTTP
// - curl_still_running: count of active transfers
// - state: current state machine position
// - assistant_response: accumulated response text
// - streaming_line_buffer: partial SSE line buffer
// - http_error_message: error from failed request
// - response_model: model name from response
// - response_finish_reason: stop reason from response
// - response_completion_tokens: token count from response
//
// Ownership: talloc child of ik_agent_ctx_t
// String fields are talloc children of this struct.
typedef struct {
    struct ik_openai_multi *multi;
    int curl_still_running;
    ik_agent_state_t state;

    // Response accumulation
    char *assistant_response;
    char *streaming_line_buffer;

    // Error and metadata
    char *http_error_message;
    char *response_model;
    char *response_finish_reason;
    int32_t response_completion_tokens;
} ik_agent_llm_t;

// Create LLM sub-context
//
// Allocates and initializes LLM state struct.
// Does NOT create curl multi handle - that's done on first request.
//
// @param ctx Talloc parent (agent context)
// @return Allocated LLM struct (never NULL - PANICs on OOM)
ik_agent_llm_t *ik_agent_llm_create(TALLOC_CTX *ctx);

// Reset LLM state for new request
//
// Clears response buffers and metadata in preparation for
// a new LLM request. Call before starting each request.
//
// @param llm LLM sub-context to reset
void ik_agent_llm_reset(ik_agent_llm_t *llm);
```

### How

1. Create `src/agent_llm.h` with struct and function declarations
2. Create `src/agent_llm.c` with implementation:

```c
#include "agent_llm.h"
#include "panic.h"

#include <assert.h>

ik_agent_llm_t *ik_agent_llm_create(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    ik_agent_llm_t *llm = talloc_zero(ctx, ik_agent_llm_t);
    if (llm == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Initialize state machine to idle
    llm->state = IK_AGENT_STATE_IDLE;

    // All other fields initialize to zero/NULL via talloc_zero:
    // - multi: NULL (created on first request)
    // - curl_still_running: 0
    // - assistant_response: NULL
    // - streaming_line_buffer: NULL
    // - http_error_message: NULL
    // - response_model: NULL
    // - response_finish_reason: NULL
    // - response_completion_tokens: 0

    return llm;
}

void ik_agent_llm_reset(ik_agent_llm_t *llm)
{
    assert(llm != NULL);  // LCOV_EXCL_BR_LINE

    // Free and clear response buffers
    if (llm->assistant_response != NULL) {
        talloc_free(llm->assistant_response);
        llm->assistant_response = NULL;
    }

    if (llm->streaming_line_buffer != NULL) {
        talloc_free(llm->streaming_line_buffer);
        llm->streaming_line_buffer = NULL;
    }

    if (llm->http_error_message != NULL) {
        talloc_free(llm->http_error_message);
        llm->http_error_message = NULL;
    }

    // Clear metadata (strings borrowed from response parsing)
    if (llm->response_model != NULL) {
        talloc_free(llm->response_model);
        llm->response_model = NULL;
    }

    if (llm->response_finish_reason != NULL) {
        talloc_free(llm->response_finish_reason);
        llm->response_finish_reason = NULL;
    }

    llm->response_completion_tokens = 0;
    llm->curl_still_running = 0;

    // Note: multi handle and state are NOT reset here
    // - multi is reused across requests
    // - state is managed by state transition functions
}
```

3. Update Makefile to compile agent_llm.c

### Why

The LLM sub-context manages HTTP streaming and response accumulation. Key design decisions:

- **State enum moved here:** The state machine is LLM-specific, not agent-general
- **Direct pointer return:** Only OOM can fail, so PANIC rather than res_t
- **Reset function:** Common pattern to clear buffers between requests
- **Multi handle not created:** Lazy initialization - created on first actual request

**Note:** The state enum is duplicated from agent.h during migration. After Phase 4, the original can be removed.

## TDD Cycle

### Red

Create `tests/unit/agent/agent_llm_test.c`:

```c
#include "agent_llm.h"
#include <check.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void) {
    test_ctx = talloc_new(NULL);
}

static void teardown(void) {
    talloc_free(test_ctx);
}

START_TEST(test_agent_llm_create_returns_non_null) {
    ik_agent_llm_t *llm = ik_agent_llm_create(test_ctx);
    ck_assert_ptr_nonnull(llm);
}
END_TEST

START_TEST(test_agent_llm_state_init_idle) {
    ik_agent_llm_t *llm = ik_agent_llm_create(test_ctx);
    ck_assert_int_eq(llm->state, IK_AGENT_STATE_IDLE);
}
END_TEST

START_TEST(test_agent_llm_fields_init_zero) {
    ik_agent_llm_t *llm = ik_agent_llm_create(test_ctx);

    ck_assert_ptr_null(llm->multi);
    ck_assert_int_eq(llm->curl_still_running, 0);
    ck_assert_ptr_null(llm->assistant_response);
    ck_assert_ptr_null(llm->streaming_line_buffer);
    ck_assert_ptr_null(llm->http_error_message);
    ck_assert_ptr_null(llm->response_model);
    ck_assert_ptr_null(llm->response_finish_reason);
    ck_assert_int_eq(llm->response_completion_tokens, 0);
}
END_TEST

START_TEST(test_agent_llm_is_child_of_ctx) {
    ik_agent_llm_t *llm = ik_agent_llm_create(test_ctx);
    ck_assert_ptr_eq(talloc_parent(llm), test_ctx);
}
END_TEST

START_TEST(test_agent_llm_reset_clears_response) {
    ik_agent_llm_t *llm = ik_agent_llm_create(test_ctx);

    // Set some fields
    llm->assistant_response = talloc_strdup(llm, "test response");
    llm->response_completion_tokens = 42;

    ik_agent_llm_reset(llm);

    ck_assert_ptr_null(llm->assistant_response);
    ck_assert_int_eq(llm->response_completion_tokens, 0);
}
END_TEST

START_TEST(test_agent_llm_reset_preserves_state) {
    ik_agent_llm_t *llm = ik_agent_llm_create(test_ctx);

    llm->state = IK_AGENT_STATE_WAITING_FOR_LLM;

    ik_agent_llm_reset(llm);

    // State should NOT be reset
    ck_assert_int_eq(llm->state, IK_AGENT_STATE_WAITING_FOR_LLM);
}
END_TEST

START_TEST(test_agent_llm_reset_handles_null_fields) {
    ik_agent_llm_t *llm = ik_agent_llm_create(test_ctx);

    // Reset with all NULL fields - should not crash
    ik_agent_llm_reset(llm);

    ck_assert_ptr_null(llm->assistant_response);
}
END_TEST
```

Run `make check` - expect test failures.

### Green

1. Create `src/agent_llm.h`
2. Create `src/agent_llm.c`
3. Add to Makefile
4. Run `make check` - all tests pass

### Verify

- `make check` passes
- `make lint` passes
- State enum values match original in agent.h

## Post-conditions

- `src/agent_llm.h` exists with struct definition
- `src/agent_llm.c` exists with factory and reset implementation
- `ik_agent_llm_create()` works correctly
- `ik_agent_llm_reset()` clears appropriate fields
- State enum defined in new header
- Tests pass for LLM creation and reset
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If issues arise:
1. Remove `src/agent_llm.h` and `src/agent_llm.c`
2. Remove test file
3. Revert Makefile changes
4. `make check` should pass (back to original state)

## Sub-agent Usage

- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits
