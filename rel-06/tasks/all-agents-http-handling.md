# Task: Process HTTP Events for All Agents

## Target

Background Agent Event Loop (gap.md) - HTTP Event Handling

## Pre-read

### Skills

- default
- tdd
- style
- errors
- quality
- scm
- database

### Source Files

- src/repl_event_handlers.c (handle_curl_events at line 261, handle_request_success at line 196, handle_request_error at line 170)
- src/repl_event_handlers.h
- src/openai/client_multi.h

### Test Patterns

- tests/unit/repl/event_handlers_test.c
- tests/integration/repl_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes
- Task `agent-repl-backpointer` is complete
- Task `agent-state-transitions` is complete
- Task `agent-callback-context` is complete
- Task `agent-tool-execution` is complete
- Task `all-agents-fd-monitoring` is complete

## Task

Refactor `handle_curl_events()` to process HTTP completions for ALL agents, and refactor `handle_request_success/error` to take an agent parameter.

### What

Currently `handle_curl_events()` only processes `repl->current`:
```c
if (repl->current->curl_still_running > 0) {
    // Process only current agent
}
```

After this task, it processes all agents with active transfers.

### How

#### Refactor `handle_request_error`

```c
// Old signature:
static void handle_request_error(ik_repl_ctx_t *repl)

// New signature:
static void handle_agent_request_error(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
```

Change all `repl->current->` to `agent->`:
- `agent->http_error_message`
- `agent->scrollback`
- `agent->assistant_response`

#### Refactor `handle_request_success`

```c
// Old signature:
void handle_request_success(ik_repl_ctx_t *repl)

// New signature:
void handle_agent_request_success(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
```

Change all `repl->current->` to `agent->`:
- `agent->assistant_response`
- `agent->conversation`
- `agent->pending_tool_call`
- `agent->tool_iteration_count`

Update calls to use agent-based functions:
- `ik_agent_start_tool_execution(agent)` (from previous task)
- `ik_agent_transition_to_idle(agent)` (from previous task)

Also update `ik_repl_should_continue_tool_loop()` to take agent:
```c
bool ik_agent_should_continue_tool_loop(const ik_agent_ctx_t *agent)
```

#### Refactor `handle_curl_events`

```c
res_t handle_curl_events(ik_repl_ctx_t *repl, int ready)
{
    (void)ready;

    // Process ALL agents with active transfers
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

                if (agent->http_error_message != NULL) {
                    handle_agent_request_error(repl, agent);
                } else {
                    handle_agent_request_success(repl, agent);
                }

                // Transition to IDLE if still WAITING_FOR_LLM
                pthread_mutex_lock_(&agent->tool_thread_mutex);
                current_state = agent->state;
                pthread_mutex_unlock_(&agent->tool_thread_mutex);

                if (current_state == IK_AGENT_STATE_WAITING_FOR_LLM) {
                    ik_agent_transition_to_idle(agent);
                }

                // Only render if this is the current agent
                if (agent == repl->current) {
                    CHECK(ik_repl_render_frame(repl));
                }
            }
        }
    }
    return OK(NULL);
}
```

#### Update `submit_tool_loop_continuation`

This static function also needs to take agent parameter since it submits HTTP requests for a specific agent.

### Why

When Agent A's HTTP request completes while user is viewing Agent B, we need to:
1. Detect the completion (from FD monitoring task)
2. Process it for Agent A specifically (this task)
3. Continue Agent A's tool loop if needed
4. Only render if Agent A is current

## TDD Cycle

### Red

Write tests that:
1. Create two agents (A and B)
2. Start HTTP request on Agent A
3. Switch repl->current to Agent B
4. Simulate Agent A's HTTP completing (mock curl_multi_perform)
5. Call handle_curl_events()
6. Verify Agent A's completion was processed
7. Verify Agent A transitioned to correct state
8. Verify Agent B was not affected

### Green

1. Refactor handle_request_error to take agent
2. Refactor handle_request_success to take agent
3. Refactor ik_repl_should_continue_tool_loop to take agent
4. Refactor submit_tool_loop_continuation to take agent
5. Update handle_curl_events to loop over all agents
6. Add conditional rendering (only for current agent)
7. Run `make check`

### Verify

- `make check` passes
- `make lint` passes

## Post-conditions

- `handle_curl_events()` processes all agents with active transfers
- `handle_agent_request_success/error()` operate on passed agent
- `ik_agent_should_continue_tool_loop()` takes agent parameter
- Rendering only happens for current agent
- Background agents can complete HTTP and continue tool loops
- All tests pass
