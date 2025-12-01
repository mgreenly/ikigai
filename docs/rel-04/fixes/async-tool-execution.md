# Fix: Async Tool Execution

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `coverage.md` - 100% coverage requirement
- `style.md` - Code style conventions
- `naming.md` - Naming conventions

## Files to Explore

### Source files:
- `src/repl.h` - REPL context (with new thread fields from fix 04)
- `src/repl.c` - Event loop, `handle_request_success`, state transitions
- `src/repl_tool.c` - `ik_repl_execute_pending_tool` (current sync implementation)
- `src/tool_dispatcher.c` - `ik_tool_dispatch` function
- `src/tool.h` - Tool types

### Prerequisite:
- Fix 04 (tool-thread-infrastructure.md) must be complete

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

The event loop is completely blocked during tool execution.

### Target Flow (Asynchronous)

```
handle_request_success()
├── Start async tool execution
│   ├── Create thread context
│   ├── Spawn worker thread
│   └── Transition to EXECUTING_TOOL state
└── Return (event loop continues)

Event loop iteration:
├── Spinner animates
├── Handle resize
├── Check tool_thread_complete flag
│   └── If complete:
│       ├── Join thread
│       ├── Steal result from thread context
│       ├── Add messages to conversation
│       ├── Display in scrollback
│       ├── Transition state
│       └── Continue tool loop if needed
```

## High-Level Goal

**Move tool dispatch to background thread so event loop remains responsive.**

### Required Changes

#### Step 1: Create thread worker function

In `src/repl_tool.c`, add:
```c
#include <pthread.h>

// Thread argument structure
typedef struct {
    TALLOC_CTX *ctx;           // Memory context for result
    const char *tool_name;     // Copied, safe to use
    const char *arguments;     // Copied, safe to use
    char *result;              // Output: result JSON
    ik_repl_ctx_t *repl;       // For signaling completion
} tool_thread_args_t;

// Worker thread function
static void *tool_thread_worker(void *arg)
{
    tool_thread_args_t *args = (tool_thread_args_t *)arg;

    // Execute tool (allocates into args->ctx)
    res_t result = ik_tool_dispatch(args->ctx, args->tool_name, args->arguments);

    // Store result (always OK with JSON string)
    args->result = result.ok;

    // Signal completion (under mutex)
    pthread_mutex_lock_(&args->repl->tool_thread_mutex);
    args->repl->tool_thread_complete = true;
    pthread_mutex_unlock_(&args->repl->tool_thread_mutex);

    return NULL;
}
```

#### Step 2: Refactor ik_repl_execute_pending_tool to start thread

Split into two functions:

```c
// Start async tool execution (called from handle_request_success)
void ik_repl_start_tool_execution(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);
    assert(repl->pending_tool_call != NULL);
    assert(!repl->tool_thread_running);

    ik_tool_call_t *tc = repl->pending_tool_call;

    // Create thread-local memory context
    repl->tool_thread_ctx = talloc_new(repl);
    if (repl->tool_thread_ctx == NULL) PANIC("Out of memory");

    // Create thread arguments (owned by thread context)
    tool_thread_args_t *args = talloc(repl->tool_thread_ctx, tool_thread_args_t);
    if (args == NULL) PANIC("Out of memory");

    args->ctx = repl->tool_thread_ctx;
    args->tool_name = talloc_strdup(repl->tool_thread_ctx, tc->name);
    args->arguments = talloc_strdup(repl->tool_thread_ctx, tc->arguments);
    args->result = NULL;
    args->repl = repl;

    // Reset completion flag
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    repl->tool_thread_complete = false;
    repl->tool_thread_running = true;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    // Spawn thread
    int ret = pthread_create_(&repl->tool_thread, NULL, tool_thread_worker, args);
    if (ret != 0) {
        // Thread creation failed - fall back to sync? Or error?
        PANIC("Failed to create tool thread");
    }

    // Transition to EXECUTING_TOOL state
    ik_repl_transition_to_executing_tool(repl);
}

// Complete async tool execution (called when thread is done)
void ik_repl_complete_tool_execution(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);
    assert(repl->tool_thread_running);
    assert(repl->tool_thread_complete);

    // Join thread
    pthread_join_(repl->tool_thread, NULL);

    ik_tool_call_t *tc = repl->pending_tool_call;

    // Get result from thread args
    // The args struct is in tool_thread_ctx, find it
    // Actually, we stored it... need to track args pointer
    // Better: store result pointer directly in repl context

    // Steal result from thread context
    char *result_json = talloc_steal(repl, repl->tool_thread_result);

    // 1. Add tool_call message to conversation
    char *summary = talloc_asprintf(repl, "%s(%s)", tc->name, tc->arguments);
    if (summary == NULL) PANIC("Out of memory");
    ik_openai_msg_t *tc_msg = ik_openai_msg_create_tool_call(
        repl->conversation, tc->id, "function", tc->name, tc->arguments, summary);
    res_t result = ik_openai_conversation_add_msg(repl->conversation, tc_msg);
    if (is_err(&result)) PANIC("allocation failed");

    // 2. Add tool result message to conversation
    ik_openai_msg_t *result_msg = ik_openai_msg_create_tool_result(
        repl->conversation, tc->id, result_json);
    result = ik_openai_conversation_add_msg(repl->conversation, result_msg);
    if (is_err(&result)) PANIC("allocation failed");

    // 3. Display in scrollback via event renderer
    ik_event_render(repl->scrollback, "tool_call", summary, "{}");
    const char *formatted_result = ik_format_tool_result(repl, tc->name, result_json);
    ik_event_render(repl->scrollback, "tool_result", formatted_result, "{}");

    // 4. Clean up
    talloc_free(summary);
    talloc_free(repl->pending_tool_call);
    repl->pending_tool_call = NULL;
    talloc_free(repl->tool_thread_ctx);
    repl->tool_thread_ctx = NULL;

    // Reset thread state
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    repl->tool_thread_running = false;
    repl->tool_thread_complete = false;
    repl->tool_thread_result = NULL;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    // Transition back to WAITING_FOR_LLM
    ik_repl_transition_from_executing_tool(repl);
}
```

**Note:** Need to refine how result is passed. Options:
- Store `tool_thread_args_t *` pointer in repl context
- Have worker write directly to `repl->tool_thread_result`

The cleaner approach: worker writes to `repl->tool_thread_result` directly (protected by the completion flag pattern - main thread only reads after seeing complete=true).

#### Step 3: Update handle_request_success

In `src/repl.c`, change:
```c
// OLD:
if (repl->pending_tool_call != NULL) {
    ik_repl_execute_pending_tool(repl);
}

// Check if we should continue the tool loop
if (ik_repl_should_continue_tool_loop(repl)) {
    repl->tool_iteration_count++;
    submit_tool_loop_continuation(repl);
}

// NEW:
if (repl->pending_tool_call != NULL) {
    // Start async execution - returns immediately
    ik_repl_start_tool_execution(repl);
    // Don't check tool loop here - will be checked when thread completes
    return;  // Exit early, state is now EXECUTING_TOOL
}

// No tool call - check if we should continue the tool loop
if (ik_repl_should_continue_tool_loop(repl)) {
    repl->tool_iteration_count++;
    submit_tool_loop_continuation(repl);
}
```

#### Step 4: Add thread completion check to event loop

In `src/repl.c` `ik_repl_run`, add check in main loop (after handling other events):

```c
// Check for tool thread completion
if (repl->state == IK_REPL_STATE_EXECUTING_TOOL) {
    bool complete = false;
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    complete = repl->tool_thread_complete;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    if (complete) {
        // Tool finished - complete execution and continue
        ik_repl_complete_tool_execution(repl);

        // Now check if tool loop should continue
        if (ik_repl_should_continue_tool_loop(repl)) {
            repl->tool_iteration_count++;
            submit_tool_loop_continuation(repl);
        } else {
            // Tool loop done, transition to IDLE
            ik_repl_transition_to_idle(repl);
        }

        // Re-render
        CHECK(ik_repl_render_frame(repl));
    }
}
```

#### Step 5: Update declarations in repl.h

```c
// Async tool execution (replaces ik_repl_execute_pending_tool)
void ik_repl_start_tool_execution(ik_repl_ctx_t *repl);
void ik_repl_complete_tool_execution(ik_repl_ctx_t *repl);
```

Remove or deprecate `ik_repl_execute_pending_tool`.

### Thread Safety Notes

1. **talloc safety**: Worker thread allocates only into `tool_thread_ctx` which main thread doesn't touch until after join
2. **Result handoff**: Worker sets `tool_thread_result`, then sets `complete=true` under mutex. Main thread only reads result after seeing `complete=true`.
3. **No concurrent access**: Clear ownership boundaries prevent races

### Testing Strategy

1. **Unit tests**: Mock `pthread_create_` to test thread spawn logic
2. **Unit tests**: Mock thread completion to test `ik_repl_complete_tool_execution`
3. **Integration tests**: Verify spinner animates during tool execution
4. **Integration tests**: Verify tool loop continues correctly after async completion

### Edge Cases

1. Thread creation failure - currently PANICs, could fall back to sync
2. Very fast tools - thread completes before main loop checks (safe, just picks up immediately)
3. Multiple tool calls in sequence - each waits for previous (sequential, not parallel)

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Spinner continues animating during tool execution
- Tool results correctly added to conversation
- Tool loop continues after async completion
- No race conditions or memory leaks
