# Task: Define ik_agent_tool_executor_t Struct

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 1 (Create Sub-Context Structs)

## Model

sonnet/thinking (8 fields, mutex initialization)

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

- src/agent.h lines 111-119 (tool execution fields to extract)
- src/tool.h (tool call type)
- src/agent.c (current tool execution initialization in ik_agent_create)

### Test Patterns

- tests/unit/agent/agent_test.c (agent creation tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_identity_t` exists (from agent-identity-struct.md)
- `ik_agent_display_t` exists (from agent-display-struct.md)
- `ik_agent_llm_t` exists (from agent-llm-struct.md)
- No `ik_agent_tool_executor_t` struct exists yet

## Task

Create the `ik_agent_tool_executor_t` struct and factory function. This sub-context extracts 8 tool execution fields from ik_agent_ctx_t. This is the most complex sub-context due to mutex initialization and thread safety concerns.

### What

Create new header `src/agent_tool_executor.h` with:

```c
#pragma once

#include "tool.h"

#include <talloc.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>

// Agent tool executor sub-context
//
// Contains all tool execution state for an agent:
// - pending: current tool call being executed (or NULL)
// - thread: pthread handle for background execution
// - mutex: synchronization for thread-safe access
// - running: true while tool thread is active
// - complete: true when thread finished (result ready)
// - ctx: talloc context for thread-local allocations
// - result: JSON result from tool execution
// - iteration_count: number of tool iterations in current turn
//
// Ownership: talloc child of ik_agent_ctx_t
// Mutex must be destroyed via talloc destructor.
//
// Thread safety:
// - running, complete, result accessed under mutex
// - pending set by main thread before thread start
// - ctx owned by thread during execution
typedef struct {
    ik_tool_call_t *pending;
    pthread_t thread;
    pthread_mutex_t mutex;
    bool running;
    bool complete;
    TALLOC_CTX *ctx;
    char *result;
    int32_t iteration_count;
} ik_agent_tool_executor_t;

// Create tool executor sub-context
//
// Allocates tool executor struct and initializes mutex.
// Registers talloc destructor to destroy mutex on free.
//
// @param ctx Talloc parent (agent context)
// @return Allocated tool executor struct (never NULL - PANICs on OOM)
ik_agent_tool_executor_t *ik_agent_tool_executor_create(TALLOC_CTX *ctx);

// Check if tool executor has a running tool
//
// Thread-safe check for active tool execution.
// Uses mutex internally for safe access.
//
// @param executor Tool executor to check
// @return true if tool is running, false otherwise
bool ik_agent_tool_executor_is_running(ik_agent_tool_executor_t *executor);

// Check if tool executor has a completed result
//
// Thread-safe check for completed tool execution.
// Uses mutex internally for safe access.
//
// @param executor Tool executor to check
// @return true if tool completed, false otherwise
bool ik_agent_tool_executor_is_complete(ik_agent_tool_executor_t *executor);
```

### How

1. Create `src/agent_tool_executor.h` with struct and function declarations
2. Create `src/agent_tool_executor.c` with implementation:

```c
#include "agent_tool_executor.h"
#include "panic.h"
#include "wrapper.h"

#include <assert.h>

// Talloc destructor to clean up mutex
static int tool_executor_destructor(ik_agent_tool_executor_t *executor)
{
    pthread_mutex_destroy_(&executor->mutex);
    return 0;
}

ik_agent_tool_executor_t *ik_agent_tool_executor_create(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    ik_agent_tool_executor_t *executor = talloc_zero(ctx, ik_agent_tool_executor_t);
    if (executor == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Initialize mutex
    int rc = pthread_mutex_init_(&executor->mutex, NULL);
    if (rc != 0) PANIC("Failed to initialize mutex");  // LCOV_EXCL_BR_LINE

    // Register destructor to clean up mutex
    talloc_set_destructor(executor, tool_executor_destructor);

    // All other fields initialize to zero/NULL via talloc_zero:
    // - pending: NULL (set when tool call arrives)
    // - thread: zero-initialized (set on thread start)
    // - running: false
    // - complete: false
    // - ctx: NULL (created per-execution)
    // - result: NULL (set by thread)
    // - iteration_count: 0

    return executor;
}

bool ik_agent_tool_executor_is_running(ik_agent_tool_executor_t *executor)
{
    assert(executor != NULL);  // LCOV_EXCL_BR_LINE

    pthread_mutex_lock_(&executor->mutex);
    bool running = executor->running;
    pthread_mutex_unlock_(&executor->mutex);

    return running;
}

bool ik_agent_tool_executor_is_complete(ik_agent_tool_executor_t *executor)
{
    assert(executor != NULL);  // LCOV_EXCL_BR_LINE

    pthread_mutex_lock_(&executor->mutex);
    bool complete = executor->complete;
    pthread_mutex_unlock_(&executor->mutex);

    return complete;
}
```

3. Update Makefile to compile agent_tool_executor.c

### Why

The tool executor is the most sensitive sub-context due to threading:

- **Mutex initialization:** Required for thread-safe state access
- **Talloc destructor:** Ensures mutex is destroyed when struct freed
- **Thread-safe accessors:** `is_running` and `is_complete` hide mutex usage
- **Wrapper functions:** Using `pthread_mutex_init_` etc. for mockability

Key design decisions:
- Direct pointer return (only OOM can fail -> PANIC)
- Destructor pattern ensures cleanup even on error paths
- Accessors centralize mutex locking pattern

**Note:** Actual tool execution (thread start/complete) remains in agent.c. This task only creates the state container.

## TDD Cycle

### Red

Create `tests/unit/agent/agent_tool_executor_test.c`:

```c
#include "agent_tool_executor.h"
#include <check.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void) {
    test_ctx = talloc_new(NULL);
}

static void teardown(void) {
    talloc_free(test_ctx);
}

START_TEST(test_tool_executor_create_returns_non_null) {
    ik_agent_tool_executor_t *executor = ik_agent_tool_executor_create(test_ctx);
    ck_assert_ptr_nonnull(executor);
}
END_TEST

START_TEST(test_tool_executor_fields_init_zero) {
    ik_agent_tool_executor_t *executor = ik_agent_tool_executor_create(test_ctx);

    ck_assert_ptr_null(executor->pending);
    ck_assert(executor->running == false);
    ck_assert(executor->complete == false);
    ck_assert_ptr_null(executor->ctx);
    ck_assert_ptr_null(executor->result);
    ck_assert_int_eq(executor->iteration_count, 0);
}
END_TEST

START_TEST(test_tool_executor_is_child_of_ctx) {
    ik_agent_tool_executor_t *executor = ik_agent_tool_executor_create(test_ctx);
    ck_assert_ptr_eq(talloc_parent(executor), test_ctx);
}
END_TEST

START_TEST(test_tool_executor_is_running_initially_false) {
    ik_agent_tool_executor_t *executor = ik_agent_tool_executor_create(test_ctx);
    ck_assert(ik_agent_tool_executor_is_running(executor) == false);
}
END_TEST

START_TEST(test_tool_executor_is_complete_initially_false) {
    ik_agent_tool_executor_t *executor = ik_agent_tool_executor_create(test_ctx);
    ck_assert(ik_agent_tool_executor_is_complete(executor) == false);
}
END_TEST

START_TEST(test_tool_executor_is_running_reflects_state) {
    ik_agent_tool_executor_t *executor = ik_agent_tool_executor_create(test_ctx);

    // Directly set running (simulating thread start)
    pthread_mutex_lock_(&executor->mutex);
    executor->running = true;
    pthread_mutex_unlock_(&executor->mutex);

    ck_assert(ik_agent_tool_executor_is_running(executor) == true);
}
END_TEST

START_TEST(test_tool_executor_is_complete_reflects_state) {
    ik_agent_tool_executor_t *executor = ik_agent_tool_executor_create(test_ctx);

    // Directly set complete (simulating thread finish)
    pthread_mutex_lock_(&executor->mutex);
    executor->complete = true;
    pthread_mutex_unlock_(&executor->mutex);

    ck_assert(ik_agent_tool_executor_is_complete(executor) == true);
}
END_TEST

START_TEST(test_tool_executor_mutex_works) {
    ik_agent_tool_executor_t *executor = ik_agent_tool_executor_create(test_ctx);

    // Test mutex can be locked and unlocked
    int rc = pthread_mutex_lock_(&executor->mutex);
    ck_assert_int_eq(rc, 0);

    rc = pthread_mutex_unlock_(&executor->mutex);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_tool_executor_destructor_runs_on_free) {
    // Create separate context for this test
    TALLOC_CTX *local_ctx = talloc_new(NULL);
    ik_agent_tool_executor_t *executor = ik_agent_tool_executor_create(local_ctx);

    // Should not crash - destructor destroys mutex
    talloc_free(local_ctx);

    // If we get here without crash, destructor worked
    ck_assert(true);
}
END_TEST
```

Run `make check` - expect test failures.

### Green

1. Create `src/agent_tool_executor.h`
2. Create `src/agent_tool_executor.c`
3. Add to Makefile
4. Run `make check` - all tests pass

### Verify

- `make check` passes
- `make lint` passes
- `make helgrind` passes (thread safety verified)
- Destructor properly destroys mutex

## Post-conditions

- `src/agent_tool_executor.h` exists with struct definition
- `src/agent_tool_executor.c` exists with factory and accessor implementation
- `ik_agent_tool_executor_create()` initializes mutex
- Talloc destructor destroys mutex on free
- Thread-safe accessors work correctly
- Tests pass for tool executor creation
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If issues arise:
1. Remove `src/agent_tool_executor.h` and `src/agent_tool_executor.c`
2. Remove test file
3. Revert Makefile changes
4. `make check` should pass (back to original state)

## Sub-agent Usage

- Use haiku sub-agents for running `make check`, `make lint`, and `make helgrind` verification
- Use haiku sub-agents for git commits
