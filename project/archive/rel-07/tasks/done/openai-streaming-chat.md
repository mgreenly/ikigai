# Task: OpenAI Chat Completions Streaming (Async)

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** openai-response-chat.md, sse-parser.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking:

- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

**Reference implementation:** `src/openai/client_multi.c` - Existing async OpenAI client

## Async Streaming Pattern

This task implements **async** streaming that integrates with the REPL's event loop:

1. `start_stream()` returns immediately after adding request to curl_multi
2. SSE parsing happens in curl write callback during `perform()`
3. Stream events delivered to user callback during `perform()`
4. Completion callback invoked from `info_read()` when transfer completes

**DO NOT** implement blocking streaming where `stream()` blocks until complete.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Existing async implementation (CRITICAL - follow this pattern):**
- `src/openai/client_multi.h` - Multi-handle manager interface with fdset/perform/timeout/info_read
- `src/openai/client_multi.c` - curl_multi wrapper implementation
- `src/openai/client_multi_internal.h` - Internal structures (active_request_t, http_write_ctx_t)
- `src/openai/client_multi_callbacks.h` - Write callback and context structures
- `src/openai/client_multi_request.c` - Request building with curl easy handle configuration

**SSE parsing:**
- `src/openai/sse_parser.h` - SSE parser for Chat Completions (data-only format)
- `src/openai/sse_parser.c` - SSE parsing implementation

**New provider abstraction (if exists):**
- `src/providers/openai/response.h` - Finish reason mapping
- `src/providers/common/sse_parser.h` - Common SSE parser (if refactored)
- `src/providers/provider.h` - Stream callback types and vtable

**Plan documents:**
- `scratch/plan/streaming.md` - Event normalization and async data flow
- `scratch/plan/provider-interface.md` - Async vtable specification (start_stream)
- `scratch/plan/architecture.md` - Event loop integration details

## Objective

Implement **async** streaming for OpenAI Chat Completions API that integrates with select()-based event loop. Handle SSE data events with `choices[].delta` format and `[DONE]` terminator. Normalize to internal streaming events via callbacks invoked during `perform()`.

## Chat Completions Stream Format

Data-only SSE stream (no event names):
```
data: {"id":"chatcmpl-123","choices":[{"delta":{"role":"assistant"}}]}

data: {"id":"chatcmpl-123","choices":[{"delta":{"content":"Hello"}}]}

data: {"id":"chatcmpl-123","choices":[{"delta":{},"finish_reason":"stop"}]}

data: [DONE]
```

Key characteristics:
- No `event:` field - only `data:` lines
- Delta objects contain incremental content
- Tool calls streamed with index-based deltas
- Terminated by literal `[DONE]` string
- Usage included in final chunk (with `stream_options.include_usage`)

## Interface

### Async Flow (Vtable Integration)

The streaming interface follows the vtable async pattern:

```c
// Start streaming request (non-blocking, returns immediately)
res_t (*start_stream)(void *ctx, const ik_request_t *req,
                      ik_stream_cb_t stream_cb, void *stream_ctx,
                      ik_provider_completion_cb_t completion_cb, void *completion_ctx);

// Event loop integration (called by REPL main loop)
res_t (*fdset)(void *ctx, fd_set *r, fd_set *w, fd_set *e, int *max_fd);
res_t (*perform)(void *ctx, int *running_handles);  // Invokes stream_cb as data arrives
void  (*info_read)(void *ctx, ik_logger_t *logger);  // Invokes completion_cb when done
```

**Callback invocation points:**
- `stream_cb` - invoked from curl write callback during `perform()` as SSE events arrive
- `completion_cb` - invoked from `info_read()` when HTTP transfer completes

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_chat_stream_ctx_t` | ctx, user_cb, user_ctx, model, finish_reason, usage, started, in_tool_call, tool_call_index, current_tool_id, current_tool_name, current_tool_args | Chat Completions streaming state |

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_chat_stream_ctx_create(ctx, cb, cb_ctx)` | Create streaming context (returns pointer, PANICs on OOM) |
| `ik_openai_chat_stream_process_data(stream_ctx, data)` | Process SSE data line - called from curl write callback |
| `ik_openai_chat_stream_get_usage(stream_ctx)` | Get accumulated usage from stream |
| `ik_openai_chat_stream_get_finish_reason(stream_ctx)` | Get finish reason from stream |

### curl Write Callback Integration

The streaming context is used within the curl write callback chain:

```c
// curl write callback (invoked during perform())
size_t http_write_callback(char *data, size_t size, size_t nmemb, void *userdata) {
    http_write_ctx_t *ctx = userdata;

    // Feed to SSE parser
    ik_sse_parse_chunk(ctx->parser, data, size * nmemb);

    // SSE parser invokes process_data for each complete event
    // process_data invokes user's stream_cb with normalized events

    return size * nmemb;
}
```

### Helper Functions (Internal)

- `emit_event(stream, event)` - Call user callback with normalized event
- `maybe_emit_start(stream)` - Emit IK_STREAM_START if not yet started
- `maybe_end_tool_call(stream)` - Emit IK_STREAM_TOOL_CALL_DONE if active
- `process_delta(stream, delta, finish_reason)` - Process choices[0].delta

## Behaviors

### Stream Initialization

- Create stream context with user callback (stream_cb)
- Initialize finish_reason to UNKNOWN
- Set started = false, in_tool_call = false
- Initialize tool_call_index = -1
- Context is owned by caller's talloc context

### Async Event Flow

1. Caller invokes `start_stream()` (non-blocking)
2. Request added to curl_multi handle
3. `start_stream()` returns immediately
4. REPL event loop runs: fdset() -> select() -> perform() -> info_read()
5. During `perform()`, curl invokes write callback as data arrives
6. Write callback feeds data to SSE parser
7. SSE parser invokes `ik_openai_chat_stream_process_data()` for each event
8. `process_data()` invokes user's `stream_cb` with normalized `ik_stream_event_t`
9. When transfer completes, `info_read()` invokes `completion_cb`

### START Event

- Emit once when first delta arrives (during `perform()`)
- Include model name from first chunk
- Set started = true after emitting

### Data Processing (invoked from write callback)

- Check for "[DONE]" terminator first
- Parse JSON if not DONE
- Extract model from first chunk
- Process choices[0].delta for content
- Extract finish_reason from choice

### Text Delta Events

- Extract content field from delta
- Emit IK_STREAM_TEXT_DELTA with text and index=0
- End any active tool call before emitting text

### Tool Call Events

- Tool calls arrive as array with index field
- Each tool call starts with id and function.name
- Arguments arrive incrementally as function.arguments strings
- Track current tool call by index
- Emit START when new tool call begins (different index)
- Emit DELTA for each arguments chunk
- Emit DONE when tool call changes or stream ends

### Tool Call State Machine

- in_tool_call = false initially
- When tool_calls delta arrives with new index:
  - End previous tool call if active (emit DONE)
  - Start new tool call (emit START with id, name)
  - Set in_tool_call = true
- When arguments delta arrives:
  - Emit DELTA with arguments chunk
- When tool call ends (new index or [DONE]):
  - Emit DONE
  - Set in_tool_call = false

### Usage Extraction

- Usage included in final chunk with stream_options.include_usage
- Extract prompt_tokens, completion_tokens, total_tokens
- Check completion_tokens_details.reasoning_tokens for thinking tokens
- Store in stream context for retrieval by completion callback

### Finish Reason

- Extract from finish_reason field in choice
- Map using `ik_openai_map_chat_finish_reason()`
- Store in stream context
- Include in DONE event

### DONE Event

- Emitted when "[DONE]" marker received
- End any active tool call first
- Include finish_reason and usage
- provider_data = NULL

### Error Handling

- Check for error field in JSON
- Emit IK_STREAM_ERROR event
- Map error type to category (authentication_error, rate_limit_error)
- Include error message

### Malformed Data

- Skip unparseable JSON chunks silently
- Don't crash on missing fields
- Log warning for debugging

## Test Scenarios

### Simple Text Stream

- Series of content deltas
- Verify START emitted once
- Verify TEXT_DELTA for each chunk
- Verify DONE emitted with finish_reason
- Test that callbacks are invoked (simulate perform() cycle)

### Tool Call Stream

- Delta with tool_calls array
- Tool call with id, name in first chunk
- Arguments deltas in subsequent chunks
- Verify START, DELTA, DELTA, DONE sequence
- Verify tool call index tracked correctly

### Multiple Tool Calls

- Two tool calls with different indices
- Verify first tool call completed before second starts
- Verify DONE emitted for first, START for second

### Usage in Final Chunk

- Stream with usage in last chunk before DONE
- Verify usage extracted correctly
- Verify included in DONE event

### Error in Stream

- JSON with error field
- Verify ERROR event emitted
- Verify category mapped (rate_limit, auth)

### DONE Marker

- Literal "[DONE]" string
- Verify DONE event emitted
- Verify any active tool call ended

### Async Integration (if testing full flow)

- Test that `start_stream()` returns immediately
- Test that events arrive during `perform()` calls
- Test that completion_cb is invoked from `info_read()`

## Implementation Notes

### Reference: Existing client_multi.c Pattern

Follow the existing pattern in `src/openai/client_multi.c`:

1. **Request setup** (in start_stream equivalent):
   - Create `active_request_t` with easy handle
   - Configure curl options (URL, headers, POST body)
   - Set write callback with `http_write_ctx_t`
   - Add easy handle to multi handle
   - Return immediately

2. **Write callback** (invoked during perform):
   - Accumulate data
   - Feed to SSE parser
   - Parser invokes event handler
   - Event handler calls user's stream_cb

3. **Completion** (in info_read):
   - `curl_multi_info_read()` returns completed transfers
   - Receive `ik_http_completion_t` from shared HTTP layer
   - Build `ik_provider_completion_t` with parsed response and metadata
   - Invoke completion_cb with provider_completion
   - Clean up curl handles

### Memory Management

- Stream context: owned by caller's talloc context
- Accumulated strings (model, finish_reason, tool_args): children of stream context
- Events passed to callback: temporary, valid only during callback
- Completion info: temporary, valid only during completion callback

## Postconditions

- [ ] `src/providers/openai/streaming.h` declares Chat streaming types and functions
- [ ] `src/providers/openai/streaming_chat.c` implements Chat streaming with async pattern
- [ ] Streaming context created via `ik_openai_chat_stream_ctx_create()`
- [ ] `ik_openai_chat_stream_process_data()` processes SSE events from write callback
- [ ] Events delivered to user callback during `perform()` (not blocking)
- [ ] Handles data-only SSE format (no event names)
- [ ] "[DONE]" marker terminates stream correctly
- [ ] Text deltas emit IK_STREAM_TEXT_DELTA events
- [ ] Tool calls emit START, DELTA, DONE events with proper state machine
- [ ] Errors emit IK_STREAM_ERROR with correct category
- [ ] Usage extracted from final chunk
- [ ] Makefile updated with streaming_chat.c
- [ ] All streaming tests pass
- [ ] Compiles without warnings
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: openai-streaming-chat.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).