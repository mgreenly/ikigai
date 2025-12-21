# Task: OpenAI Shim Streaming Implementation

**Layer:** 4
**Model:** sonnet/extended
**Depends on:** openai-shim-send.md, anthropic-streaming.md, google-streaming.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load memory` - talloc ownership patterns
- `/load errors` - Result types
- `/load source-code` - Callback structures

**Source:**
- `src/providers/provider.h` - ik_stream_event_t, ik_stream_callback_t
- `src/openai/client_multi.h` - ik_openai_multi_t, streaming callbacks
- `src/openai/client_multi_callbacks.c` - http_write_callback implementation
- `src/openai/sse_parser.h` - SSE event parsing
- `src/providers/anthropic/` - Reference streaming implementation (by this layer)
- `src/providers/google/` - Reference streaming implementation (by this layer)

**Plan:**
- `scratch/plan/streaming.md` - Normalized stream event types

## Objective

Implement streaming support for the OpenAI shim by wrapping the existing SSE-based streaming callbacks. This is deferred to Layer 4 because: (1) it's the highest-risk piece, (2) by Layer 4, Anthropic and Google streaming are implemented providing reference patterns, (3) sync-only operation is functional for initial validation.

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_shim_stream(void *impl_ctx, const ik_request_t *req, ik_stream_callback_t cb, void *user_ctx)` | Vtable stream implementation |

### Internal Helpers

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_shim_wrap_stream_cb(const char *chunk, void *ctx)` | Adapter callback for existing streamer |
| `void ik_openai_shim_emit_event(ik_stream_event_t *event, ik_stream_callback_t cb, void *user_ctx)` | Emit normalized event to user callback |

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_shim_stream_ctx_t` | user_cb, user_ctx, accumulated_text, tool_call_state, event_count | Context for callback wrapper |

### Files to Update

- `src/providers/openai/shim.c` - Replace stub stream() with real implementation
- `src/providers/openai/shim.h` - Add streaming types

## Behaviors

### Streaming Flow

1. Cast `impl_ctx` to `ik_openai_shim_ctx_t*`
2. Create wrapper context with user callback/ctx
3. Transform request to legacy format
4. Call `ik_openai_multi_add_request()` with wrapper callback
5. Wrapper callback converts each chunk to normalized events
6. On completion, emit DONE event with finish_reason and usage

### Callback Wrapper

The existing `ik_openai_stream_cb_t` receives raw text chunks:
```c
typedef res_t (*ik_openai_stream_cb_t)(const char *chunk, void *ctx);
```

The wrapper must:
1. Receive text chunk
2. Create `ik_stream_event_t` with type=TEXT_DELTA
3. Call user's normalized callback
4. Track accumulated state for final DONE event

### Event Type Mapping

| Existing Behavior | Normalized Event |
|-------------------|------------------|
| First chunk received | START (with model name) |
| Text chunk | TEXT_DELTA |
| Tool call detected (from completion) | TOOL_CALL_START, then TOOL_CALL_DONE |
| Stream complete | DONE (with finish_reason, usage) |
| Error mid-stream | ERROR |

### Tool Call Streaming

The existing code accumulates tool calls in `http_write_ctx_t`:
- Tool call chunks accumulate in `ctx->tool_call->arguments`
- Final tool call available in completion callback

For normalized streaming:
1. Detect tool call start from first tool chunk
2. Emit TOOL_CALL_START with id, name, index=0
3. Emit TOOL_CALL_DELTA for each arguments chunk
4. Emit TOOL_CALL_DONE when complete

Challenge: Existing code doesn't expose intermediate tool chunks cleanly. May need to:
- Parse SSE events in wrapper to detect tool calls
- Or accept that tool calls arrive as single event (not streamed)

### Completion Callback Integration

The existing `ik_http_completion_cb_t` receives:
- finish_reason
- model
- completion_tokens
- tool_call (if present)

Wrapper must intercept completion to emit:
1. TOOL_CALL_START/DONE if tool_call present
2. DONE with finish_reason and usage

### Error Handling

- Mid-stream errors: emit ERROR event, stop streaming
- Callback errors: propagate ERR, stop streaming
- Network errors: emit ERROR event with category=NETWORK

### Memory Management

- Wrapper context allocated on temporary talloc context
- Events allocated on wrapper context, copied by user if needed
- Free wrapper context after streaming completes
- Handle cleanup on error paths

## Test Scenarios

### Unit Tests
- Stream simple text: receives START, TEXT_DELTA(s), DONE
- Stream with tool call: receives START, TOOL_CALL events, DONE
- Mid-stream error: receives ERROR event
- Callback returns error: streaming stops

### Integration Tests
- Compare streaming output to sync output: final content matches
- Verify event ordering: START always first, DONE always last
- Verify tool call events contain correct data

### Comparison with Other Providers
- Same normalized events for equivalent Anthropic/Google streams
- Event types used consistently across providers

## Postconditions

- [ ] `stream()` vtable function works end-to-end
- [ ] Text streaming emits correct events
- [ ] Tool call streaming emits correct events
- [ ] Error handling emits ERROR event
- [ ] Event ordering is correct
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] No changes to `src/openai/` files
- [ ] All existing tests still pass
