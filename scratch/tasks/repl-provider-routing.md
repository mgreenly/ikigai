# Task: Update REPL to Route via Provider Vtable (Async)

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** openai-shim-send.md, agent-provider-fields.md, configuration.md, request-builders.md, provider-types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

The application uses a **select()-based event loop**. ALL HTTP operations MUST be non-blocking:

- Use async vtable methods (fdset, perform, timeout, info_read, start_stream)
- Expose provider FDs for select() integration
- NEVER block the main thread
- Stream callbacks invoked during `perform()`, not as blocking return values

**Reference:** The existing `src/openai/client_multi.c` already implements this pattern. The REPL event loop in `src/repl.c` and `src/repl_event_handlers.c` shows the current integration.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load source-code` - Map of REPL implementation
- `/load errors` - Result type patterns

**Source (Current Implementation - Read These First):**
- `src/repl.c` - Main event loop with select(), lines 30-104. Shows:
  - `ik_repl_setup_fd_sets()` call for FD setup
  - `ik_repl_calculate_curl_min_timeout()` for timeout
  - `posix_select_()` call
  - `ik_repl_handle_curl_events()` for processing
- `src/repl_event_handlers.c` - Event handlers, key functions:
  - `ik_repl_setup_fd_sets()` - Currently calls `ik_openai_multi_fdset()` per agent
  - `ik_repl_calculate_curl_min_timeout()` - Currently calls `ik_openai_multi_timeout()`
  - `ik_repl_handle_curl_events()` - Currently calls `ik_openai_multi_perform()` and `ik_openai_multi_info_read()`
- `src/repl_actions_llm.c` - LLM request submission (send_to_llm_):
  - Currently calls `ik_openai_multi_add_request()` directly
- `src/agent.h` - Agent structure with multi handle

**Plan (Async Architecture):**
- `scratch/plan/README.md` - Critical constraint: select()-based event loop
- `scratch/plan/architecture.md` - Event Loop Integration (CRITICAL) section with code example
- `scratch/plan/provider-interface.md` - REPL integration details, vtable specification
- `scratch/plan/streaming.md` - REPL consumption flow, async streaming architecture

## Objective

Update REPL to route all LLM requests through the provider vtable instead of directly calling `ik_openai_multi_*` functions. The provider vtable exposes the same async pattern (fdset/perform/timeout/info_read/start_stream) so integration is a matter of indirection through the vtable.

**Key insight:** The existing code already uses the correct async pattern. This task changes:
1. Where FDs come from (agent->provider->vt->fdset instead of ik_openai_multi_fdset)
2. How requests are started (agent->provider->vt->start_stream instead of ik_openai_multi_add_request)
3. How I/O is processed (agent->provider->vt->perform/info_read instead of ik_openai_multi_*)

## Interface

Functions to update:

| Function | Location | Changes |
|----------|----------|---------|
| `ik_repl_setup_fd_sets` | `src/repl_event_handlers.c` | Call `agent->provider->vt->fdset()` instead of `ik_openai_multi_fdset()` |
| `ik_repl_calculate_curl_min_timeout` | `src/repl_event_handlers.c` | Call `agent->provider->vt->timeout()` instead of `ik_openai_multi_timeout()` |
| `process_agent_curl_events` | `src/repl_event_handlers.c` | Call `agent->provider->vt->perform()` and `info_read()` instead of `ik_openai_multi_*` |
| `send_to_llm_` | `src/repl_actions_llm.c` | Call `agent->provider->vt->start_stream()` instead of `ik_openai_multi_add_request()` |
| `submit_tool_loop_continuation` | `src/repl_event_handlers.c` | Call `agent->provider->vt->start_stream()` instead of `ik_openai_multi_add_request()` |

Structs to update:

| Struct | Change |
|--------|--------|
| `ik_agent_ctx_t` | Replace `ik_openai_multi_t *multi` with `ik_provider_t *provider` |

## Behaviors

### Event Loop Integration (CRITICAL)

The REPL main loop integrates with providers via select(). Replace direct OpenAI calls with vtable dispatch:

**Before (current):**
```c
// In ik_repl_setup_fd_sets:
res_t result = ik_openai_multi_fdset(agent->multi, read_fds, write_fds, exc_fds, &agent_max_fd);

// In ik_repl_calculate_curl_min_timeout:
CHECK(ik_openai_multi_timeout(repl->agents[i]->multi, &agent_timeout));

// In process_agent_curl_events:
CHECK(ik_openai_multi_perform(agent->multi, &agent->curl_still_running));
ik_openai_multi_info_read(agent->multi, repl->shared->logger);
```

**After (provider vtable):**
```c
// In ik_repl_setup_fd_sets:
if (agent->provider != NULL) {
    res_t result = agent->provider->vt->fdset(agent->provider->ctx,
                                               read_fds, write_fds, exc_fds, &agent_max_fd);
    if (is_err(&result)) return result;
}

// In ik_repl_calculate_curl_min_timeout:
if (repl->agents[i]->provider != NULL) {
    CHECK(repl->agents[i]->provider->vt->timeout(repl->agents[i]->provider->ctx, &agent_timeout));
}

// In process_agent_curl_events:
if (agent->provider != NULL) {
    CHECK(agent->provider->vt->perform(agent->provider->ctx, &agent->curl_still_running));
    agent->provider->vt->info_read(agent->provider->ctx, repl->shared->logger);
}
```

### Request Initiation (start_stream)

Replace `ik_openai_multi_add_request()` with `provider->vt->start_stream()`:

**Before (current in send_to_llm_):**
```c
result = ik_openai_multi_add_request(repl->current->multi, repl->shared->cfg, repl->current->conversation,
                                     ik_repl_streaming_callback, repl->current,
                                     ik_repl_http_completion_callback, repl->current, false,
                                     repl->shared->logger);
```

**After (provider vtable):**
```c
// Get or create provider (lazy initialization)
ik_provider_t *provider = NULL;
TRY(ik_agent_get_provider(agent, &provider));

// Build normalized request from conversation
ik_request_t *req = NULL;
TRY(ik_request_build_from_conversation(ctx, agent, &req));

// Start async stream (returns immediately)
result = provider->vt->start_stream(provider->ctx, req,
                                    ik_repl_streaming_callback, agent,
                                    ik_repl_http_completion_callback, agent);
if (is_ok(&result)) {
    agent->curl_still_running = 1;
}
```

### Request Building

Build `ik_request_t` from agent conversation state:
- Model from `agent->model`
- System messages from `agent->system_prompt`
- Conversation history from `agent->conversation->messages`
- Tool definitions if any
- Thinking level from `agent->thinking_level`

This is handled by `ik_request_build_from_conversation()` helper (defined in request-builders.md task).

### Callback Handling (Unchanged Pattern)

Callbacks are already invoked correctly:
- `ik_repl_streaming_callback` - Stream events during `perform()`
- `ik_repl_http_completion_callback` - Completion during `info_read()`

The callback signatures remain the same. Providers invoke them during perform/info_read.

### Agent Initialization

1. Call `ik_agent_apply_defaults(agent, config)` for new root agent
2. Agent gets `config->default_provider` on first creation
3. Provider created lazily on first `ik_agent_get_provider()` call
4. Forked agents inherit provider from parent (handled in agent.c)
5. Restored agents load provider from database (handled in agent.c)

### Error Handling

- Return `ERR_MISSING_CREDENTIALS` if provider creation fails
- Return `ERR_INVALID_ARG` if agent is NULL
- Pass through provider vtable errors
- Errors delivered via completion callback, not from start_stream()

## Test Scenarios

### Event Loop Integration
- FD sets populated from provider vtable
- Timeout calculated from provider vtable
- perform() called after select() returns
- info_read() called after perform()
- Stream callbacks invoked during perform()
- Completion callbacks invoked during info_read()

### Request Routing
- start_stream() returns immediately (non-blocking)
- Stream events delivered via callback during perform()
- Completion delivered via callback during info_read()
- Missing credentials: returns `ERR_MISSING_CREDENTIALS`
- NULL agent: returns `ERR_INVALID_ARG`

### Agent Initialization
- New root agent: gets config default provider
- Config with "anthropic" default: agent uses anthropic
- Agent has provider field set correctly
- Provider created lazily on first use

### Tool Loop Continuation
- submit_tool_loop_continuation uses provider vtable
- Tool results sent via start_stream()
- Continuation respects async pattern

## Postconditions

- [ ] `ik_repl_setup_fd_sets()` uses `agent->provider->vt->fdset()`
- [ ] `ik_repl_calculate_curl_min_timeout()` uses `agent->provider->vt->timeout()`
- [ ] `process_agent_curl_events()` uses `agent->provider->vt->perform()` and `info_read()`
- [ ] `send_to_llm_()` uses `agent->provider->vt->start_stream()`
- [ ] `submit_tool_loop_continuation()` uses `agent->provider->vt->start_stream()`
- [ ] `ik_agent_ctx_t` has `provider` field instead of `multi`
- [ ] No direct `ik_openai_multi_*` calls remain in REPL routing code
- [ ] Compiles without warnings
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: repl-provider-routing.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).