# Task: Refactor Callbacks to Use Agent Context

## Target

Background Agent Event Loop (gap.md) - Callback Context Bug

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

- src/repl_callbacks.c (callback implementations)
- src/repl_callbacks.h (callback declarations)
- src/repl_actions_llm.c (call site, line 131-134)
- src/repl_event_handlers.c (call site, lines 244-248)
- src/commands_fork.c (call site, lines 132-136)
- src/openai/client_multi.h (callback types)

### Test Patterns

- tests/unit/repl/callbacks_test.c (if exists)
- tests/integration/repl_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes
- Task `agent-repl-backpointer` is complete
- Task `agent-state-transitions` is complete

## Task

Refactor HTTP callbacks to receive `ik_agent_ctx_t *agent` as context instead of `ik_repl_ctx_t *repl`, and update all call sites.

### What

1. Change streaming callback to use agent context
2. Change completion callback to use agent context
3. Update all `ik_openai_multi_add_request()` call sites to pass agent
4. Remove render call from streaming callback

### How

#### Streaming Callback (`ik_repl_streaming_callback`)

Change from:
```c
ik_repl_ctx_t *repl = (ik_repl_ctx_t *)ctx;
// Uses repl->current->assistant_response, repl->current->scrollback, etc.
ik_repl_render_frame(repl);
```

To:
```c
ik_agent_ctx_t *agent = (ik_agent_ctx_t *)ctx;
// Uses agent->assistant_response, agent->scrollback, etc.
// NO render call - event loop handles rendering
```

#### Completion Callback (`ik_repl_http_completion_callback`)

Change from:
```c
ik_repl_ctx_t *repl = (ik_repl_ctx_t *)ctx;
// Uses repl->current->http_error_message, repl->current->response_model, etc.
```

To:
```c
ik_agent_ctx_t *agent = (ik_agent_ctx_t *)ctx;
// Uses agent->http_error_message, agent->response_model, etc.
// Access shared via agent->shared for logging
```

#### Call Sites

Update all `ik_openai_multi_add_request()` calls:

```c
// Old:
ik_openai_multi_add_request(..., ik_repl_streaming_callback, repl,
                                  ik_repl_http_completion_callback, repl, ...);

// New:
ik_openai_multi_add_request(..., ik_repl_streaming_callback, repl->current,
                                  ik_repl_http_completion_callback, repl->current, ...);
```

For commands_fork.c, pass the child agent, not repl->current.

### Why

Current callbacks use `repl->current` which changes when user switches agents. If Agent A initiates HTTP, then user switches to Agent B, Agent A's callbacks update Agent B's state - corrupting data.

Removing render from streaming callback is necessary because:
1. Background agents shouldn't trigger renders (not visible)
2. Event loop renders once per iteration for the current agent
3. Cleaner separation of concerns

## TDD Cycle

### Red

Write tests that:
1. Create two agents (A and B)
2. Set repl->current to agent B
3. Call streaming callback with agent A as context
4. Verify agent A's assistant_response is updated (not agent B's)
5. Verify no crash or unexpected behavior

Similar tests for completion callback.

### Green

1. Update streaming callback to use agent context
2. Update completion callback to use agent context
3. Update all three call sites to pass agent
4. Remove ik_repl_render_frame() from streaming callback
5. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- Verify callback updates correct agent in test

## Post-conditions

- Callbacks receive `ik_agent_ctx_t *agent` as context
- Callbacks update `agent->` fields, not `repl->current->`
- Streaming callback does not call `ik_repl_render_frame()`
- All call sites pass the correct agent to callbacks
- All tests pass
