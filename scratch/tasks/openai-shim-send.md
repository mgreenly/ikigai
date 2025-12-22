# Task: OpenAI Shim Async Request Implementation

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** openai-shim-request.md, openai-shim-response.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

**Critical Architecture Constraint:** The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking. This task implements async `start_request()` pattern, NOT blocking `send()`.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] `src/providers/openai/shim.h` exists with shim context types
- [ ] Request/response transform functions exist from prior tasks

## Pre-Read

**Skills:**
- `/load memory` - talloc ownership patterns
- `/load errors` - Result types
- `/load source-code` - OpenAI client structure (especially client_multi.c)

**Source:**
- `src/providers/openai/shim.h` - Shim context and transforms
- `src/openai/client_multi.c` - Existing async multi-handle implementation (REFERENCE PATTERN)
- `src/openai/client_multi_request.c` - ik_openai_multi_add_request() implementation
- `src/openai/client_multi_callbacks.c` - Completion callback handling
- `src/openai/http_handler.h` - HTTP types and callbacks
- `src/config.h` - ik_cfg_t structure

**Plan:**
- `scratch/plan/README.md` - Critical constraint: select()-based event loop
- `scratch/plan/provider-interface.md` - Async vtable specification (fdset, perform, start_request)
- `scratch/plan/architecture.md` - Adapter shim migration strategy
- `scratch/plan/streaming.md` - Callback patterns for async events

## Objective

Implement the async `start_request()` vtable function for the OpenAI shim. This wires together the request transform with the existing `ik_openai_multi_add_request()` function, enabling non-blocking request initiation that integrates with the REPL's select()-based event loop.

**Key insight:** The existing `src/openai/client_multi.c` already implements the correct async pattern. The shim exposes this async interface through the provider vtable.

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_shim_start_request(void *impl_ctx, const ik_request_t *req, ik_provider_completion_cb_t cb, void *cb_ctx)` | Async vtable start_request implementation |

### Internal Helpers

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_shim_build_cfg(TALLOC_CTX *ctx, ik_openai_shim_ctx_t *shim, const ik_request_t *req, ik_cfg_t **out)` | Build temporary config for existing client |

### Callback Wrapper

| Function | Purpose |
|----------|---------|
| `static res_t shim_completion_wrapper(const ik_provider_completion_t *completion, void *ctx)` | Wraps internal callback to transform response before invoking user callback |

### Files to Update

- `src/providers/openai/shim.c` - Implement start_request() and event loop methods
- `src/providers/openai/shim.h` - Add declarations if needed

## Behaviors

### Async start_request() Flow

1. Cast `impl_ctx` to `ik_openai_shim_ctx_t*`
2. Transform request: `ik_openai_shim_transform_request()`
3. Build temporary `ik_cfg_t` with api_key and model from request
4. Create wrapper context to capture user callback and transform response
5. Call `ik_openai_multi_add_request()` with wrapper callback
6. **Return immediately** (non-blocking) - do NOT wait for response

The actual HTTP I/O happens later when the REPL event loop calls:
- `fdset()` - to get curl_multi FDs for select()
- `perform()` - to process pending I/O after select() returns
- `info_read()` - to check for completed transfers and invoke callbacks

### Event Loop Integration Methods

The shim must also implement these vtable methods that delegate to existing multi-handle:

```c
res_t ik_openai_shim_fdset(void *ctx, fd_set *r, fd_set *w, fd_set *e, int *max_fd);
res_t ik_openai_shim_perform(void *ctx, int *running_handles);
res_t ik_openai_shim_timeout(void *ctx, long *timeout_ms);
void  ik_openai_shim_info_read(void *ctx, ik_logger_t *logger);
```

These delegate directly to corresponding `ik_openai_multi_*` functions on the shim's multi-handle.

### Completion Callback Flow

When transfer completes (detected in `info_read()`):
1. Internal completion callback receives raw OpenAI response
2. Transform response: `ik_openai_shim_transform_response()`
3. Create `ik_http_completion_t` with normalized response
4. Invoke user's completion callback with transformed response
5. User callback processes result (saves to DB, updates UI, etc.)

### Config Building

The existing client expects `ik_cfg_t` with:
- `openai_api_key` - From shim context
- `openai_model` - From request->model
- `openai_temperature` - Default 0.7 (or from request if available)
- `openai_max_completion_tokens` - From request->max_output_tokens

Create minimal config struct on temporary context.

### Error Propagation

**From start_request():**
- Transform errors: return immediately (before adding to multi)
- Config build errors: return immediately
- Add request errors: return immediately

**From completion callback:**
- HTTP errors: wrap in `ik_http_completion_t` with error info
- Transform errors: wrap in completion with error info
- Invoke user callback with error completion (do NOT return error from callback)

### Memory Management

- Wrapper context allocated with request lifetime
- Response transformation allocates on wrapper context
- User callback can steal response to own context
- Wrapper context freed after user callback returns
- talloc hierarchy handles cleanup on error paths

### Vtable Registration

Update factory function to register async methods:
```c
vtable->fdset = ik_openai_shim_fdset;
vtable->perform = ik_openai_shim_perform;
vtable->timeout = ik_openai_shim_timeout;
vtable->info_read = ik_openai_shim_info_read;
vtable->start_request = ik_openai_shim_start_request;
vtable->start_stream = ik_openai_shim_start_stream_stub;  // Still stub
vtable->cleanup = ik_openai_shim_destroy;
```

## Test Scenarios

### Unit Tests (Mocked curl_multi)

The tests must simulate the async fdset/perform/info_read cycle:

- **start_request returns immediately**: Verify function returns OK before HTTP completes
- **Callback invoked from info_read**: Start request, simulate perform/info_read cycle, verify callback fires
- **Text response transform**: Verify response transformed correctly in callback
- **Tool call response**: Verify tool calls parsed and transformed correctly
- **Missing API key**: start_request returns ERR_MISSING_CREDENTIALS immediately
- **Empty messages**: start_request returns ERR_INVALID_ARG immediately
- **HTTP error in callback**: Verify error completion delivered to user callback

### Test Harness Pattern

```c
// 1. Start request (returns immediately)
res_t res = provider->vt->start_request(ctx, &req, my_callback, &my_ctx);
CHECK(is_ok(&res));

// 2. Simulate event loop cycle
int running = 1;
while (running > 0) {
    fd_set r, w, e;
    int max_fd;
    provider->vt->fdset(ctx, &r, &w, &e, &max_fd);
    // In real code: select() here
    // In test: mock performs instant completion
    provider->vt->perform(ctx, &running);
    provider->vt->info_read(ctx, NULL);
}

// 3. Verify callback was invoked with expected data
ck_assert(my_ctx.callback_invoked);
ck_assert_ptr_nonnull(my_ctx.response);
```

### Integration Tests (With Mock Server)

- Full async request/response cycle through vtable
- Verify fdset returns correct FDs
- Verify perform processes data correctly
- Verify info_read triggers callback at completion

## Postconditions

- [ ] `start_request()` vtable function returns immediately (non-blocking)
- [ ] `fdset()`, `perform()`, `timeout()`, `info_read()` delegate to multi-handle
- [ ] Request transform + existing client + response transform integrated
- [ ] Completion callback receives transformed response
- [ ] Errors from start_request returned immediately
- [ ] Errors from HTTP delivered via completion callback
- [ ] No memory leaks on success or error paths
- [ ] Unit tests pass with simulated async cycle
- [ ] `make check` passes
- [ ] No changes to `src/openai/` files
- [ ] Existing OpenAI tests still pass
- [ ] Changes committed to git with message: `task: openai-shim-send.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).