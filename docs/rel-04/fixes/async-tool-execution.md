# Fix: Async Tool Execution

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `coverage.md` - 100% coverage requirement
- `style.md` - Code style conventions
- `naming.md` - Naming conventions

## Files to Explore

### Source files:
- `src/repl.h` - REPL context (with new thread fields from infrastructure fix)
- `src/repl.c` - Event loop, `handle_request_success`, state transitions
- `src/repl_tool.c` - `ik_repl_execute_pending_tool` (current sync implementation)
- `src/tool_dispatcher.c` - `ik_tool_dispatch` function
- `src/tool.h` - Tool types

### Prerequisite:
- `tool-thread-infrastructure.md` must be complete first

## Situation

### Current Flow (Synchronous)

```
handle_request_success()
├── ik_repl_execute_pending_tool(repl)  ← BLOCKS
│   ├── Add tool_call msg to conversation
│   ├── ik_tool_dispatch()  ← Can take seconds/minutes
│   ├── Add tool_result msg to conversation
│   └── Display in scrollback
└── Check should_continue_tool_loop
```

**Problem:** The event loop is completely blocked during tool execution. No spinner animation, no resize handling, no Ctrl+C responsiveness.

### Target Flow (Asynchronous)

```
handle_request_success()
├── Start async tool execution
│   ├── Create thread context
│   ├── Spawn worker thread
│   └── Transition to EXECUTING_TOOL state
└── Return immediately (event loop continues)

Event loop iteration (while thread runs):
├── Spinner animates
├── Handle terminal resize
├── Check tool_thread_complete flag
│   └── If complete:
│       ├── Join thread (instant - it's already done)
│       ├── Harvest result from repl->tool_thread_result
│       ├── Add messages to conversation
│       ├── Display in scrollback
│       ├── Transition state
│       └── Continue tool loop if needed
```

## High-Level Goal

**Move tool dispatch to background thread so event loop remains responsive.**

## Design Decisions

### D1: Result Passing via repl->tool_thread_result

The worker thread writes directly to `repl->tool_thread_result` rather than returning via thread args.

**Why:** The worker already accesses `repl->tool_thread_complete` to signal completion. Using `repl->tool_thread_result` is consistent - both are "output" fields in repl that the worker writes and main thread reads.

**Thread safety:** The completion flag pattern guarantees safety:
1. Worker writes result
2. Worker sets `complete=true` under mutex
3. Main thread sees `complete=true`
4. Main thread reads result (safe - worker is done writing)

No mutex needed for the result itself - the flag ordering provides the synchronization barrier.

### D2: PANIC on Thread Creation Failure

Thread creation failure triggers PANIC rather than falling back to synchronous execution.

**Why:** Thread creation only fails under severe resource exhaustion (thousands of threads, out of memory). At that point:
- The system is in serious trouble anyway
- Falling back to sync would duplicate completion logic (complexity)
- The edge case essentially never happens in practice

**Trade-off:** A stuck system PANICs instead of degrading gracefully. Acceptable for rel-04; can revisit if real-world usage shows this matters.

### D3: Ctrl+C Handled by Destructor

Ctrl+C cleanup is handled by the talloc destructor in the infrastructure fix, not here.

**Why:** The destructor joins any running thread before freeing resources. This provides a single cleanup path for all exit scenarios (normal exit, Ctrl+C, errors). No special signal handler logic needed in this fix.

**Known limitation:** If a bash command is stuck, Ctrl+C waits for it. This is documented in README's "Out of Scope" section as "Bash command timeout - not implemented".

## Required Changes

### Step 1: Create thread worker function

In `src/repl_tool.c`, add:

```c
#include <pthread.h>

// Arguments passed to the worker thread.
// All strings are copied into tool_thread_ctx so the thread owns them.
// The repl pointer is used to write results back - this is safe because
// main thread only reads these fields after seeing tool_thread_complete=true.
typedef struct {
    TALLOC_CTX *ctx;           // Memory context for allocations (owned by main thread)
    const char *tool_name;     // Copied into ctx, safe for thread to use
    const char *arguments;     // Copied into ctx, safe for thread to use
    ik_repl_ctx_t *repl;       // For writing result and signaling completion
} tool_thread_args_t;

// Worker thread function - runs tool dispatch in background.
//
// Thread safety model:
// - Worker WRITES to repl->tool_thread_result (no mutex - see D1 above)
// - Worker WRITES to repl->tool_thread_complete UNDER MUTEX
// - Main thread only READS result AFTER seeing complete=true
// - The mutex on the flag provides the memory barrier
static void *tool_thread_worker(void *arg)
{
    tool_thread_args_t *args = (tool_thread_args_t *)arg;

    // Execute tool - this is the potentially slow operation.
    // All allocations go into args->ctx which main thread will free.
    res_t result = ik_tool_dispatch(args->ctx, args->tool_name, args->arguments);

    // Store result directly in repl context.
    // Safe without mutex: main thread won't read until complete=true,
    // and setting complete=true (below) provides the memory barrier.
    args->repl->tool_thread_result = result.ok;

    // Signal completion under mutex.
    // The mutex ensures main thread sees result before or after this,
    // never during. Combined with the read-after-complete pattern,
    // this guarantees main thread sees the final result value.
    pthread_mutex_lock_(&args->repl->tool_thread_mutex);
    args->repl->tool_thread_complete = true;
    pthread_mutex_unlock_(&args->repl->tool_thread_mutex);

    return NULL;
}
```

### Step 2: Split execute function into start/complete

Replace `ik_repl_execute_pending_tool` with two functions:

```c
// Start async tool execution - spawns thread and returns immediately.
// Called from handle_request_success when LLM returns a tool call.
//
// After this returns:
// - State is EXECUTING_TOOL
// - Thread is running (or we PANICed)
// - Event loop resumes, spinner animates
void ik_repl_start_tool_execution(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);                    // LCOV_EXCL_BR_LINE
    assert(repl->pending_tool_call != NULL); // LCOV_EXCL_BR_LINE
    assert(!repl->tool_thread_running);      // LCOV_EXCL_BR_LINE

    ik_tool_call_t *tc = repl->pending_tool_call;

    // Create memory context for thread allocations.
    // Owned by main thread (child of repl), freed after completion.
    // Thread allocates into this but doesn't free it.
    repl->tool_thread_ctx = talloc_new(repl);
    if (repl->tool_thread_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Create thread arguments in the thread context.
    // Copy strings so thread has its own copies (original may be freed).
    tool_thread_args_t *args = talloc(repl->tool_thread_ctx, tool_thread_args_t);
    if (args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    args->ctx = repl->tool_thread_ctx;
    args->tool_name = talloc_strdup(repl->tool_thread_ctx, tc->name);
    args->arguments = talloc_strdup(repl->tool_thread_ctx, tc->arguments);
    args->repl = repl;

    if (args->tool_name == NULL || args->arguments == NULL) {
        PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Spawn thread BEFORE setting running=true.
    // If spawn fails, we PANIC without corrupting state.
    int ret = pthread_create_(&repl->tool_thread, NULL, tool_thread_worker, args);
    if (ret != 0) {
        // Thread creation failure is rare (resource exhaustion).
        // PANIC is acceptable for rel-04 - see design decision D2.
        // The alternative (fallback to sync) adds complexity for an
        // edge case that essentially never happens.
        PANIC("Failed to create tool thread: %s", strerror(ret)); // LCOV_EXCL_LINE
    }

    // Thread started successfully - now set flags.
    // Order matters: set running AFTER successful pthread_create.
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    repl->tool_thread_complete = false;
    repl->tool_thread_running = true;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    // Transition to EXECUTING_TOOL state.
    // Spinner stays visible, input stays hidden.
    ik_repl_transition_to_executing_tool(repl);
}

// Complete async tool execution - harvest result after thread finishes.
// Called from event loop when tool_thread_complete is true.
//
// Preconditions:
// - tool_thread_running == true
// - tool_thread_complete == true (thread is done)
//
// After this returns:
// - Messages added to conversation
// - Scrollback updated
// - Thread context freed
// - State back to WAITING_FOR_LLM
void ik_repl_complete_tool_execution(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);                  // LCOV_EXCL_BR_LINE
    assert(repl->tool_thread_running);     // LCOV_EXCL_BR_LINE
    assert(repl->tool_thread_complete);    // LCOV_EXCL_BR_LINE

    // Join thread - it's already done, so this returns immediately.
    // We still call join to clean up thread resources.
    pthread_join_(repl->tool_thread, NULL);

    ik_tool_call_t *tc = repl->pending_tool_call;

    // Steal result from thread context before freeing it.
    // talloc_steal moves ownership to repl so it survives context free.
    char *result_json = talloc_steal(repl, repl->tool_thread_result);

    // 1. Add tool_call message to conversation
    char *summary = talloc_asprintf(repl, "%s(%s)", tc->name, tc->arguments);
    if (summary == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_openai_msg_t *tc_msg = ik_openai_msg_create_tool_call(
        repl->conversation, tc->id, "function", tc->name, tc->arguments, summary);
    res_t result = ik_openai_conversation_add_msg(repl->conversation, tc_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // 2. Add tool result message to conversation
    ik_openai_msg_t *result_msg = ik_openai_msg_create_tool_result(
        repl->conversation, tc->id, result_json);
    result = ik_openai_conversation_add_msg(repl->conversation, result_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // 3. Display in scrollback via event renderer
    ik_event_render(repl->scrollback, "tool_call", summary, "{}");
    const char *formatted_result = ik_format_tool_result(repl, tc->name, result_json);
    ik_event_render(repl->scrollback, "tool_result", formatted_result, "{}");

    // 4. Clean up
    talloc_free(summary);
    talloc_free(repl->pending_tool_call);
    repl->pending_tool_call = NULL;

    // Free thread context (includes args struct and copied strings).
    // result_json was stolen out, so it survives.
    talloc_free(repl->tool_thread_ctx);
    repl->tool_thread_ctx = NULL;

    // Reset thread state for next tool call
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    repl->tool_thread_running = false;
    repl->tool_thread_complete = false;
    repl->tool_thread_result = NULL;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    // Transition back to WAITING_FOR_LLM.
    // Caller will check if tool loop should continue.
    ik_repl_transition_from_executing_tool(repl);
}
```

### Step 3: Update handle_request_success

In `src/repl.c`, change the tool execution path:

```c
// OLD (synchronous - blocks event loop):
if (repl->pending_tool_call != NULL) {
    ik_repl_execute_pending_tool(repl);
}
// Check if we should continue the tool loop
if (ik_repl_should_continue_tool_loop(repl)) {
    repl->tool_iteration_count++;
    submit_tool_loop_continuation(repl);
}

// NEW (asynchronous - returns immediately):
if (repl->pending_tool_call != NULL) {
    // Start async execution - spawns thread, transitions to EXECUTING_TOOL.
    // Returns immediately so event loop continues (spinner, resize, etc.)
    ik_repl_start_tool_execution(repl);

    // Don't check tool loop here - that happens in event loop when
    // thread completes. Exit early to return control to event loop.
    return;
}

// No tool call in this response - check if we should continue the loop.
// (This path is taken when finish_reason="tool_calls" but we just
// finished processing a tool result, OR when finish_reason="stop".)
if (ik_repl_should_continue_tool_loop(repl)) {
    repl->tool_iteration_count++;
    submit_tool_loop_continuation(repl);
}
```

### Step 4: Add thread completion polling to event loop

In `src/repl.c` `ik_repl_run`, add check in main loop after handling other events:

```c
// Poll for tool thread completion.
// This is checked every iteration while in EXECUTING_TOOL state.
// Polling is simple and sufficient - the loop runs anyway for spinner
// animation, so checking a bool under mutex is negligible overhead.
if (repl->state == IK_REPL_STATE_EXECUTING_TOOL) {
    // Check completion flag under mutex
    bool complete = false;
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    complete = repl->tool_thread_complete;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    if (complete) {
        // Thread finished - harvest result and continue.
        // This joins the thread, adds messages, updates scrollback.
        ik_repl_complete_tool_execution(repl);

        // Now check if tool loop should continue.
        // This is the same logic that was in handle_request_success,
        // moved here because completion now happens asynchronously.
        if (ik_repl_should_continue_tool_loop(repl)) {
            repl->tool_iteration_count++;
            submit_tool_loop_continuation(repl);
            // State will transition to WAITING_FOR_LLM when request starts
        } else {
            // Tool loop done - transition to IDLE, show input prompt
            ik_repl_transition_to_idle(repl);
        }

        // Re-render to show tool result in scrollback
        CHECK(ik_repl_render_frame(repl));
    }
}
```

### Step 5: Update declarations in repl.h

```c
// Async tool execution (replaces synchronous ik_repl_execute_pending_tool)
void ik_repl_start_tool_execution(ik_repl_ctx_t *repl);
void ik_repl_complete_tool_execution(ik_repl_ctx_t *repl);
```

Remove or mark deprecated: `ik_repl_execute_pending_tool`.

## Thread Safety Summary

| Field | Written by | Read by | Protection |
|-------|------------|---------|------------|
| `tool_thread_result` | Worker | Main (after complete) | Completion flag ordering |
| `tool_thread_complete` | Worker | Main | Mutex |
| `tool_thread_running` | Main only | Main, Destructor | Mutex (for destructor) |
| `tool_thread_ctx` | Main only | Main only | Single-threaded access |
| `pending_tool_call` | Main only | Main only | Single-threaded access |

**Key insight:** Most synchronization comes from the completion flag pattern, not mutexes on every field. The mutex protects the flag; the flag ordering protects everything else.

## Testing Strategy

1. **Unit tests**: Mock `pthread_create_` to verify thread spawn logic
2. **Unit tests**: Mock thread completion to test `ik_repl_complete_tool_execution`
3. **Integration tests**: Verify spinner animates during tool execution
4. **Integration tests**: Verify tool loop continues correctly after async completion
5. **Integration tests**: Verify Ctrl+C during tool execution cleans up properly

## Edge Cases

| Case | Behavior | Notes |
|------|----------|-------|
| Thread creation fails | PANIC | Rare (resource exhaustion), acceptable for rel-04 |
| Very fast tool | Works | Thread completes before next poll, picked up immediately |
| Ctrl+C during tool | Waits for thread | Destructor joins thread, then cleanup proceeds |
| Multiple sequential tools | Each waits | Only one thread at a time, sequential execution |

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Spinner continues animating during tool execution
- Tool results correctly added to conversation
- Tool loop continues after async completion
- Ctrl+C during tool execution exits cleanly (after tool finishes)
- No race conditions or memory leaks
