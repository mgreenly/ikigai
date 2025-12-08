# Fix: Tool Thread Infrastructure

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
- `src/repl.h` - REPL context struct and state enum
- `src/repl_init.c` - REPL initialization
- `src/repl.c` - State transition functions
- `src/wrapper.h` - POSIX wrapper patterns (for pthread wrappers)
- `src/wrapper.c` - Wrapper implementations

### Reference:
- `man pthread_create`, `pthread_mutex_init`, `pthread_join`

## Situation

### Current State

The REPL has two states:
```c
typedef enum {
    IK_REPL_STATE_IDLE,
    IK_REPL_STATE_WAITING_FOR_LLM
} ik_repl_state_t;
```

Tool execution happens synchronously in `handle_request_success`, blocking the event loop.

### Goal

Add infrastructure for running tools in a background thread:
- New state for tool execution
- Thread-related fields in REPL context
- Mutex for thread synchronization
- Proper init/cleanup

This fix adds the infrastructure only. The next fix (05-async-tool-execution.md) wires up the actual async dispatch.

## High-Level Goal

**Add thread infrastructure to REPL context for async tool execution.**

### Required Changes

#### Step 1: Add pthread wrappers to wrapper.h/wrapper.c

In `src/wrapper.h`, add:
```c
// Pthread wrappers (for testability)
MOCKABLE int pthread_mutex_init_(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
MOCKABLE int pthread_mutex_destroy_(pthread_mutex_t *mutex);
MOCKABLE int pthread_mutex_lock_(pthread_mutex_t *mutex);
MOCKABLE int pthread_mutex_unlock_(pthread_mutex_t *mutex);
MOCKABLE int pthread_create_(pthread_t *thread, const pthread_attr_t *attr,
                             void *(*start_routine)(void *), void *arg);
MOCKABLE int pthread_join_(pthread_t thread, void **retval);
```

In `src/wrapper.c`, add implementations that call the real functions.

#### Step 2: Add new REPL state

In `src/repl.h`, update the enum:
```c
typedef enum {
    IK_REPL_STATE_IDLE,
    IK_REPL_STATE_WAITING_FOR_LLM,
    IK_REPL_STATE_EXECUTING_TOOL    // NEW: Tool running in background thread
} ik_repl_state_t;
```

#### Step 3: Add thread fields to REPL context

In `src/repl.h`, add to `ik_repl_ctx_t`:
```c
// Tool thread execution (async tool dispatch)
pthread_t tool_thread;              // Worker thread handle
pthread_mutex_t tool_thread_mutex;  // Protects tool_thread_* fields
bool tool_thread_running;           // Thread is active
bool tool_thread_complete;          // Thread finished, result ready
TALLOC_CTX *tool_thread_ctx;        // Memory context for thread (owned by main)
char *tool_thread_result;           // Result JSON from tool dispatch
```

Add include at top:
```c
#include <pthread.h>
```

#### Step 4: Initialize mutex in repl_init.c

In `src/repl_init.c` `ik_repl_init`, after other initialization:
```c
// Initialize tool thread mutex
int mutex_ret = pthread_mutex_init_(&repl->tool_thread_mutex, NULL);
if (mutex_ret != 0) {
    return ERR(ctx, IK_ERR_SYSTEM, "Failed to initialize tool thread mutex");
}

// Initialize tool thread state
repl->tool_thread_running = false;
repl->tool_thread_complete = false;
repl->tool_thread_ctx = NULL;
repl->tool_thread_result = NULL;
```

#### Step 5: Add cleanup via talloc destructor

Need to destroy mutex on REPL cleanup. The destructor also handles Ctrl+C gracefully by joining any running thread before cleanup.

Check if there's an existing destructor. If so, augment it. If not, create one:

```c
static int repl_destructor(ik_repl_ctx_t *repl)
{
    // If tool thread is running, wait for it to finish before cleanup.
    // This handles Ctrl+C gracefully - we don't cancel the thread or leave
    // it orphaned. If a bash command is stuck, we wait (known limitation:
    // "Bash command timeout - not implemented" in README Out of Scope).
    // The alternative (pthread_cancel) risks leaving resources in bad state.
    if (repl->tool_thread_running) {
        pthread_join_(repl->tool_thread, NULL);
    }

    pthread_mutex_destroy_(&repl->tool_thread_mutex);
    return 0;
}
```

And in init (after mutex init succeeds):
```c
talloc_set_destructor(repl, repl_destructor);
```

**Design note:** The destructor is the single cleanup point for both normal exit and Ctrl+C. No special signal handler logic needed for the thread - when `talloc_free(repl)` is called (from any exit path), the destructor ensures the thread is joined before resources are freed.

#### Step 6: Add state transition functions

In `src/repl.c`, add:
```c
void ik_repl_transition_to_executing_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->state == IK_REPL_STATE_WAITING_FOR_LLM);   /* LCOV_EXCL_BR_LINE */

    repl->state = IK_REPL_STATE_EXECUTING_TOOL;
    // Spinner stays visible (already visible from WAITING_FOR_LLM)
    // Input stays hidden
}

void ik_repl_transition_from_executing_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->state == IK_REPL_STATE_EXECUTING_TOOL);   /* LCOV_EXCL_BR_LINE */

    // Transition back to WAITING_FOR_LLM (tool loop will continue)
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;
}
```

Add declarations to `src/repl.h`:
```c
void ik_repl_transition_to_executing_tool(ik_repl_ctx_t *repl);
void ik_repl_transition_from_executing_tool(ik_repl_ctx_t *repl);
```

### Testing Strategy

1. Test mutex init/destroy via REPL init/cleanup
2. Test state transitions (new state enum value)
3. Test that new fields are properly initialized to safe defaults
4. Wrapper functions need coverage (call real pthread functions)

### Memory Management

- `tool_thread_mutex` is embedded in struct (not pointer), destroyed in destructor
- `tool_thread_ctx` will be created/freed per tool execution (next fix)
- `tool_thread_result` will be stolen from thread context (next fix)

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- New state `IK_REPL_STATE_EXECUTING_TOOL` exists
- REPL context has thread fields initialized
- Mutex properly initialized and destroyed
- State transition functions exist and are tested
