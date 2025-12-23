# Task: OpenAI Shim Streaming Implementation

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/extended
**Depends on:** openai-shim-send.md, provider-types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

**This is the KEY INSIGHT for this task:**

The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking. OpenAI already has a complete async implementation via `ik_openai_multi_*` functions in `src/openai/client_multi.c`. The shim adapter's job is to **delegate to these existing functions**, implementing the vtable async interface by wrapping them rather than hiding async behind blocking calls.

**The shim does NOT implement its own HTTP/streaming logic. It DELEGATES to existing async code.**

## Pre-Read

**Skills:**
- `/load memory` - talloc ownership patterns
- `/load errors` - Result types

**Source (read all before implementing):**
- `src/openai/client_multi.h` - Existing async API: fdset, perform, timeout, info_read, add_request
- `src/openai/client_multi.c` - Async implementation to delegate to
- `src/openai/client_multi_callbacks.c` - Existing callback handling (http_write_callback)
- `src/openai/sse_parser.h` - SSE event parsing (already used by client_multi)
- `src/providers/provider.h` - Vtable interface with async methods (by provider-types.md)
- `src/providers/openai/shim.h` - Shim types (by openai-shim-send.md)

**Plan:**
- `scratch/plan/README.md` - Critical constraint: select()-based event loop
- `scratch/plan/provider-interface.md` - Async vtable specification
- `scratch/plan/streaming.md` - Normalized stream event types

## Objective

Implement streaming support for the OpenAI shim by **delegating to the existing async `ik_openai_multi_*` functions**. The shim is a thin adapter layer that:

1. Implements vtable async methods (fdset, perform, timeout, info_read, start_stream)
2. Delegates to existing `ik_openai_multi_*` functions
3. Wraps callbacks to translate between legacy and normalized event formats

**The shim does NOT re-implement async HTTP. It wraps existing async code with the new vtable interface.**

## Interface

### Vtable Methods to Implement

The shim implements these async vtable methods by delegating to `ik_openai_multi_*`:

| Vtable Method | Delegates To | Purpose |
|---------------|--------------|---------|
| `fdset(ctx, read_fds, write_fds, exc_fds, max_fd)` | `ik_openai_multi_fdset()` | Get FDs for select() |
| `perform(ctx, running_handles)` | `ik_openai_multi_perform()` | Process pending I/O |
| `timeout(ctx, timeout_ms)` | `ik_openai_multi_timeout()` | Get select() timeout |
| `info_read(ctx, logger)` | `ik_openai_multi_info_read()` | Handle completed transfers |
| `start_stream(ctx, req, stream_cb, stream_ctx, completion_cb, completion_ctx)` | `ik_openai_multi_add_request()` | Start streaming request |

### Callback Wrapper Functions

| Function | Purpose |
|----------|---------|
| `shim_stream_wrapper(chunk, ctx)` | Wraps `ik_openai_stream_cb_t` to emit normalized events to `ik_stream_cb_t` |
| `shim_completion_wrapper(completion, ctx)` | Wraps `ik_provider_completion_cb_t` to emit DONE event and call user's completion_cb |

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_shim_stream_ctx_t` | user_stream_cb, user_stream_ctx, user_completion_cb, user_completion_ctx, has_started, shim_ctx | Wrapper context for callback translation |

### Files to Modify

- `src/providers/openai/shim.c` - Add vtable method implementations and callback wrappers
- `src/providers/openai/shim.h` - Add stream wrapper types if needed

## Behaviors

### Vtable Delegation Pattern

The shim vtable methods are thin wrappers:

```
fdset(ctx, ...):
    shim = cast ctx to ik_openai_shim_ctx_t
    return ik_openai_multi_fdset(shim->multi, ...)

perform(ctx, ...):
    shim = cast ctx to ik_openai_shim_ctx_t
    return ik_openai_multi_perform(shim->multi, ...)

timeout(ctx, ...):
    shim = cast ctx to ik_openai_shim_ctx_t
    return ik_openai_multi_timeout(shim->multi, ...)

info_read(ctx, logger):
    shim = cast ctx to ik_openai_shim_ctx_t
    ik_openai_multi_info_read(shim->multi, logger)
```

### start_stream Implementation

```
start_stream(ctx, req, stream_cb, stream_ctx, completion_cb, completion_ctx):
    1. Cast ctx to ik_openai_shim_ctx_t
    2. Transform req to legacy ik_openai_conversation_t format (use existing shim transform)
    3. Create ik_openai_shim_stream_ctx_t wrapper with user callbacks
    4. Call ik_openai_multi_add_request(multi, cfg, conv,
                                         shim_stream_wrapper, wrapper_ctx,
                                         shim_completion_wrapper, wrapper_ctx,
                                         limit_reached, logger)
    5. Return immediately (non-blocking)
```

### Stream Callback Wrapper

The existing `ik_openai_stream_cb_t` receives raw text chunks:
```c
typedef res_t (*ik_openai_stream_cb_t)(const char *chunk, void *ctx);
```

The wrapper translates to normalized events:

```
shim_stream_wrapper(chunk, ctx):
    wrapper = cast ctx to ik_openai_shim_stream_ctx_t

    // Emit START on first chunk
    if (!wrapper->has_started):
        event.type = IK_STREAM_START
        event.data.start.model = wrapper->shim_ctx->cfg->model
        wrapper->user_stream_cb(&event, wrapper->user_stream_ctx)
        wrapper->has_started = true

    // Emit TEXT_DELTA for the chunk
    event.type = IK_STREAM_TEXT_DELTA
    event.data.text_delta.text = chunk
    event.data.text_delta.index = 0
    return wrapper->user_stream_cb(&event, wrapper->user_stream_ctx)
```

### Completion Callback Wrapper

The existing `ik_provider_completion_cb_t` receives `ik_provider_completion_t` with:
- finish_reason
- model
- completion_tokens
- tool_call (if present)

The wrapper emits tool call events and DONE:

```
shim_completion_wrapper(completion, ctx):
    wrapper = cast ctx to ik_openai_shim_stream_ctx_t

    // If tool call present, emit TOOL_CALL_START + TOOL_CALL_DONE
    // (Tool calls arrive complete in OpenAI streaming, not chunked)
    if (completion->tool_call != NULL):
        start_event.type = IK_STREAM_TOOL_CALL_START
        start_event.data.tool_call_start.id = completion->tool_call->id
        start_event.data.tool_call_start.name = completion->tool_call->name
        start_event.data.tool_call_start.index = 0
        wrapper->user_stream_cb(&start_event, wrapper->user_stream_ctx)

        done_event.type = IK_STREAM_TOOL_CALL_DONE
        done_event.data.tool_call_done.index = 0
        wrapper->user_stream_cb(&done_event, wrapper->user_stream_ctx)

    // Emit DONE with metadata
    done_event.type = IK_STREAM_DONE
    done_event.data.done.finish_reason = completion->finish_reason
    done_event.data.done.usage.completion_tokens = completion->completion_tokens
    wrapper->user_stream_cb(&done_event, wrapper->user_stream_ctx)

    // Call user's completion callback
    return wrapper->user_completion_cb(completion, wrapper->user_completion_ctx)
```

### Error Handling

- Errors from `ik_openai_multi_*` functions propagate through vtable methods
- HTTP/network errors delivered via completion callback (type != IK_HTTP_SUCCESS)
- On error: emit IK_STREAM_ERROR event before calling user's completion_cb
- Wrapper context freed after completion callback

### Memory Management

- Wrapper context allocated on shim context as talloc child
- Events allocated on stack (caller copies if needed to persist)
- Wrapper context freed after completion callback invoked
- Leverage existing client_multi memory management for curl handles

## Test Scenarios

### Unit Tests

- **Vtable delegation**: Each vtable method correctly calls corresponding ik_openai_multi_* function
- **Stream wrapper**: Text chunks translated to TEXT_DELTA events
- **START event**: Emitted on first chunk
- **Completion with tool call**: TOOL_CALL_START + TOOL_CALL_DONE events emitted
- **Completion without tool call**: Only DONE event emitted
- **Error handling**: HTTP errors translated to IK_STREAM_ERROR event

### Integration Tests

- **End-to-end streaming**: Start stream, receive text deltas, get completion
- **Tool call streaming**: Receive tool call in completion, events emitted correctly
- **Compare with direct ik_openai_multi usage**: Same final content

### Mock Pattern

Mock `ik_openai_multi_add_request` to inject test responses. Use VCR infrastructure from vcr-core.md (core recording/playback) and vcr-mock-integration.md (curl hook integration).

## Postconditions

- [ ] Vtable async methods implemented (fdset, perform, timeout, info_read, start_stream)
- [ ] All vtable methods delegate to ik_openai_multi_* functions
- [ ] Stream callback wrapper emits normalized events
- [ ] START event emitted on first chunk
- [ ] TEXT_DELTA events emitted for text chunks
- [ ] TOOL_CALL_START/DONE events emitted when tool call in completion
- [ ] DONE event emitted with finish_reason and usage
- [ ] Error events emitted on failures
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] No changes to existing `src/openai/` files (shim wraps, does not modify)
- [ ] All existing tests still pass
- [ ] Changes committed to git with message: `task: openai-shim-streaming.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
