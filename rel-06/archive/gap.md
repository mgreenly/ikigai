# Background Agent Event Loop

When a user switches away from an agent, that agent's LLM interaction and tool execution must continue in the background until it reaches a natural stopping point (IDLE state, waiting for user input).

## Problem Statement

The event loop only monitors `repl->current`. When user switches agents, the previous agent freezes completely.

### Issue 1: Event Loop Only Monitors Current Agent

| Location | Problem |
|----------|---------|
| `repl_event_handlers.c:84` | `setup_fd_sets()` only adds `repl->current->multi` FDs to select() |
| `repl_event_handlers.c:34-37` | `calculate_select_timeout_ms()` only checks `repl->current->state` for tool polling timeout |
| `repl_event_handlers.c:266` | `handle_curl_events()` only processes `repl->current->curl_still_running` |
| `repl.c:117-125` | Tool polling only checks `repl->current->tool_thread_complete` |

### Issue 2: Callback Context Bug (Critical)

Callbacks receive `repl` as context and use `repl->current` to find the target agent:

```c
// repl_callbacks.c:29-34
ik_repl_ctx_t *repl = (ik_repl_ctx_t *)ctx;
if (repl->current->assistant_response == NULL) {
    repl->current->assistant_response = talloc_strdup(repl, chunk);
```

When Agent A initiates HTTP, then user switches to Agent B, Agent A's completion callback updates Agent B's state (corrupts wrong agent).

### Failure Scenario

1. Agent A sends message to LLM
2. LLM responds with tool call (e.g., `bash: make check`)
3. Tool starts executing in background thread
4. User switches to Agent B
5. Agent A's tool completes - **not detected** (event loop doesn't poll)
6. Or Agent A's HTTP response arrives - **updates Agent B's state** (callback bug)
7. Agent A is stuck; Agent B is corrupted

## Design

### Principle

Callbacks update agent state. Rendering is the event loop's responsibility.

### Callback Context

Pass `ik_agent_ctx_t *agent` to callbacks instead of `ik_repl_ctx_t *repl`.

Add backpointer to agent structure:

```c
// agent.h
typedef struct ik_agent_ctx {
    // ... existing fields ...
    struct ik_repl_ctx_t *repl;  // Back-pointer for callback access
} ik_agent_ctx_t;
```

Callbacks access shared resources via `agent->shared` and repl via `agent->repl`.

### Event Loop Changes

#### `setup_fd_sets()` - Add ALL Agents' FDs

```c
res_t setup_fd_sets(ik_repl_ctx_t *repl, ...)
{
    // Terminal fd
    FD_SET(terminal_fd, read_fds);
    int max_fd = terminal_fd;

    // Add curl_multi fds for ALL agents
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        int agent_max_fd = -1;
        res_t result = ik_openai_multi_fdset(agent->multi, read_fds, write_fds, exc_fds, &agent_max_fd);
        if (is_err(&result)) return result;
        if (agent_max_fd > max_fd) max_fd = agent_max_fd;
    }

    *max_fd_out = max_fd;
    return OK(NULL);
}
```

#### `calculate_select_timeout_ms()` - Consider ALL Agents

```c
long calculate_select_timeout_ms(ik_repl_ctx_t *repl, long min_curl_timeout_ms)
{
    // Spinner: only for current (visible)
    long spinner_timeout_ms = repl->current->spinner_state.visible ? 80 : -1;

    // Tool polling: check ALL agents
    long tool_poll_timeout_ms = -1;
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        pthread_mutex_lock_(&agent->tool_thread_mutex);
        bool executing = (agent->state == IK_AGENT_STATE_EXECUTING_TOOL);
        pthread_mutex_unlock_(&agent->tool_thread_mutex);
        if (executing) {
            tool_poll_timeout_ms = 50;
            break;
        }
    }

    // ... collect minimum of all timeouts ...
}
```

Note: The curl timeout parameter must also be computed across ALL agents (call `ik_openai_multi_timeout()` for each, take minimum).

#### `handle_curl_events()` - Process ALL Agents

```c
res_t handle_curl_events(ik_repl_ctx_t *repl, int ready)
{
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];

        if (agent->curl_still_running > 0) {
            int prev_running = agent->curl_still_running;
            CHECK(ik_openai_multi_perform(agent->multi, &agent->curl_still_running));
            ik_openai_multi_info_read(agent->multi, repl->shared->logger);

            pthread_mutex_lock_(&agent->tool_thread_mutex);
            ik_agent_state_t current_state = agent->state;
            pthread_mutex_unlock_(&agent->tool_thread_mutex);

            if (prev_running > 0 && agent->curl_still_running == 0 &&
                current_state == IK_AGENT_STATE_WAITING_FOR_LLM) {
                // Handle completion for THIS agent
                handle_agent_http_completion(repl, agent);
            }
        }
    }
    return OK(NULL);
}
```

#### Tool Polling - Check ALL Agents

```c
// In ik_repl_run()
for (size_t i = 0; i < repl->agent_count; i++) {
    ik_agent_ctx_t *agent = repl->agents[i];

    pthread_mutex_lock_(&agent->tool_thread_mutex);
    ik_agent_state_t state = agent->state;
    bool complete = agent->tool_thread_complete;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    if (state == IK_AGENT_STATE_EXECUTING_TOOL && complete) {
        handle_agent_tool_completion(repl, agent);
    }
}
```

### Callback Changes

#### Streaming Callback

```c
res_t ik_repl_streaming_callback(const char *chunk, void *ctx)
{
    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)ctx;

    // Update THIS agent's state (not repl->current)
    if (agent->assistant_response == NULL) {
        agent->assistant_response = talloc_strdup(agent, chunk);
    } else {
        agent->assistant_response = talloc_strdup_append(agent->assistant_response, chunk);
    }

    // Update agent's scrollback with streaming content
    // ... line buffering logic using agent->streaming_line_buffer ...
    // ... append to agent->scrollback ...

    // DO NOT render here - event loop handles rendering
    return OK(NULL);
}
```

#### Completion Callback

```c
res_t ik_repl_http_completion_callback(const ik_http_completion_t *completion, void *ctx)
{
    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)ctx;

    // Update THIS agent's state (not repl->current)
    // ... flush agent->streaming_line_buffer to agent->scrollback ...
    // ... store agent->http_error_message ...
    // ... store agent->response_model, agent->response_finish_reason ...
    // ... store agent->pending_tool_call ...

    return OK(NULL);
}
```

### Rendering Strategy

1. Remove `ik_repl_render_frame(repl)` call from streaming callback
2. Event loop renders once per iteration, after all event processing
3. Only `repl->current` is rendered (background agents update silently)
4. When user switches to a background agent, accumulated scrollback is displayed

### State Transition Refactoring

Refactor transition functions to take `ik_agent_ctx_t *agent`:

```c
void ik_agent_transition_to_waiting_for_llm(ik_agent_ctx_t *agent)
{
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(agent->state == IK_AGENT_STATE_IDLE);
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // Always update UI state (correct when agent becomes current)
    agent->spinner_state.visible = true;
    agent->input_buffer_visible = false;
}

void ik_agent_transition_to_idle(ik_agent_ctx_t *agent)
{
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(agent->state == IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->state = IK_AGENT_STATE_IDLE;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    agent->spinner_state.visible = false;
    agent->input_buffer_visible = true;
}
```

### Request Submission

When submitting HTTP request, pass agent as callback context:

```c
// In submit_tool_loop_continuation() and similar
res_t result = ik_openai_multi_add_request(
    agent->multi,
    repl->shared->cfg,
    agent->conversation,
    ik_repl_streaming_callback,
    agent,  // <-- Pass agent, not repl
    ik_repl_http_completion_callback,
    agent,  // <-- Pass agent, not repl
    limit_reached,
    repl->shared->logger
);
```

## Natural Stopping Points

Agent returns to IDLE (waits for user input) when:

| Condition | Location |
|-----------|----------|
| `finish_reason` != "tool_calls" | `handle_request_success()` |
| Tool iteration limit reached | `ik_repl_should_continue_tool_loop()` |
| HTTP error occurs | `handle_request_error()` |
| LLM responds with text only | `handle_request_success()` (no pending_tool_call) |

These are already implemented - they just need to operate on the correct agent.

## Files Affected

| File | Changes |
|------|---------|
| `src/agent.h` | Add `repl` backpointer field |
| `src/agent.c` | Initialize `repl` backpointer in `ik_agent_create()` |
| `src/repl.c` | Tool polling loop over all agents |
| `src/repl_event_handlers.h` | Update function signatures |
| `src/repl_event_handlers.c` | `setup_fd_sets()`, `calculate_select_timeout_ms()`, `handle_curl_events()` |
| `src/repl_callbacks.c` | Update callbacks to use agent context |
| `src/repl_actions_llm.c` | Pass agent to `ik_openai_multi_add_request()` |

## Testing Strategy

1. **Unit tests**: Mock multi-agent scenarios with concurrent HTTP/tool states
2. **Integration tests**: Verify agent switch during active tool execution
3. **State corruption test**: Verify callback updates correct agent after switch

## Complexity

Medium - event loop refactor affecting ~3-4 files, but changes are localized and mechanical.
