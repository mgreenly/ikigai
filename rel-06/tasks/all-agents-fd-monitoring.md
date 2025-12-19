# Task: Monitor All Agents' File Descriptors

## Target

Background Agent Event Loop (gap.md) - FD Monitoring

## Pre-read

### Skills

- default
- tdd
- style
- errors
- quality
- scm

### Source Files

- src/repl_event_handlers.c (setup_fd_sets at line 67, calculate_select_timeout_ms at line 28)
- src/repl_event_handlers.h
- src/repl.c (event loop, curl timeout calculation at line 57)
- src/openai/client_multi.h (ik_openai_multi_fdset, ik_openai_multi_timeout)

### Test Patterns

- tests/unit/repl/event_handlers_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes
- Task `agent-repl-backpointer` is complete

## Task

Update `setup_fd_sets()` and timeout calculation to monitor ALL agents, not just `repl->current`.

### What

Currently:
- `setup_fd_sets()` only adds `repl->current->multi` FDs
- Curl timeout only considers `repl->current->multi`
- Tool poll timeout only checks `repl->current->state`

After this task:
- `setup_fd_sets()` adds FDs for ALL agents in `repl->agents[]`
- Curl timeout is minimum across ALL agents
- Tool poll timeout triggers if ANY agent is executing a tool

### How

#### `setup_fd_sets()` Changes

```c
res_t setup_fd_sets(ik_repl_ctx_t *repl, ...)
{
    FD_ZERO(read_fds);
    FD_ZERO(write_fds);
    FD_ZERO(exc_fds);

    // Terminal fd
    int32_t terminal_fd = repl->shared->term->tty_fd;
    FD_SET(terminal_fd, read_fds);
    int max_fd = terminal_fd;

    // Add curl_multi fds for ALL agents
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        int agent_max_fd = -1;
        res_t result = ik_openai_multi_fdset(agent->multi, read_fds, write_fds, exc_fds, &agent_max_fd);
        if (is_err(&result)) return result;
        if (agent_max_fd > max_fd) {
            max_fd = agent_max_fd;
        }
    }

    *max_fd_out = max_fd;
    return OK(NULL);
}
```

#### Curl Timeout Changes (in repl.c event loop)

```c
// Calculate minimum curl timeout across ALL agents
long curl_timeout_ms = -1;
for (size_t i = 0; i < repl->agent_count; i++) {
    long agent_timeout = -1;
    CHECK(ik_openai_multi_timeout(repl->agents[i]->multi, &agent_timeout));
    if (agent_timeout >= 0) {
        if (curl_timeout_ms < 0 || agent_timeout < curl_timeout_ms) {
            curl_timeout_ms = agent_timeout;
        }
    }
}
```

#### Tool Poll Timeout Changes (in calculate_select_timeout_ms)

```c
// Tool polling: check ALL agents for executing tools
long tool_poll_timeout_ms = -1;
for (size_t i = 0; i < repl->agent_count; i++) {
    ik_agent_ctx_t *agent = repl->agents[i];
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    bool executing = (agent->state == IK_AGENT_STATE_EXECUTING_TOOL);
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    if (executing) {
        tool_poll_timeout_ms = 50;
        break;  // Found one, no need to check more
    }
}
```

### Why

When user switches to Agent B while Agent A has active HTTP or tool, we still need select() to wake up when Agent A's operations complete. Without this, Agent A freezes until user switches back.

## TDD Cycle

### Red

Write tests that:
1. Create REPL with two agents
2. Agent A has active curl transfer (curl_still_running > 0)
3. Agent B is idle
4. Set repl->current to Agent B
5. Call setup_fd_sets()
6. Verify Agent A's curl FDs are in the fd_sets
7. Call timeout calculation
8. Verify timeout considers Agent A's curl timeout

### Green

1. Modify setup_fd_sets() to loop over all agents
2. Modify curl timeout calculation in repl.c
3. Modify tool poll timeout in calculate_select_timeout_ms
4. Run `make check`

### Verify

- `make check` passes
- `make lint` passes

## Post-conditions

- `setup_fd_sets()` includes FDs from ALL agents' curl_multi handles
- Curl timeout is minimum across all agents
- Tool poll timeout triggers if any agent is executing a tool
- select() wakes up when background agents have events
- All tests pass
