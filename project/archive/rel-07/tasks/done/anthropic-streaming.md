# Task: Anthropic Async Streaming Implementation

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided in this file.

**Model:** sonnet/thinking
**Depends on:** anthropic-response.md, sse-parser.md, http-client.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

**Critical Architecture Constraint:** The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking. Use curl_multi (NOT curl_easy), expose fdset() for select() integration, expose perform() for incremental processing, and NEVER block the main thread.

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - Talloc-based memory management

**Source:**
- `src/providers/anthropic/response.h` - Response parsing functions
- `src/providers/common/sse_parser.h` - SSE parser interface
- `src/providers/common/http_multi.h` - Async HTTP multi-handle interface
- `src/providers/provider.h` - Provider vtable and callback types

**Plan:**
- `scratch/plan/streaming.md` - Async streaming architecture, callback flow, event types
- `scratch/plan/provider-interface.md` - start_stream() specification, callback signatures
- `scratch/plan/architecture.md` - Provider adapter flow during perform()

## Objective

Implement async streaming for Anthropic API that integrates with the select()-based event loop. Parse Anthropic SSE events and emit normalized `ik_stream_event_t` events through callbacks. The implementation must be non-blocking: `start_stream()` returns immediately after adding the request to curl_multi, SSE parsing happens in curl write callbacks during `perform()`, stream events are delivered to the user callback during `perform()`, and completion is signaled via `info_read()`.

## Async Architecture

### Data Flow During Streaming

```
1. REPL calls provider->vt->start_stream(ctx, req, stream_cb, stream_ctx, completion_cb, completion_ctx)
   |
   v
2. Anthropic adapter:
   - Creates streaming context with user callbacks
   - Serializes request to JSON with stream:true
   - Configures curl easy handle with write callback
   - Adds easy handle to curl_multi
   - Returns immediately (NON-BLOCKING)
   |
   v
3. REPL event loop calls provider->vt->perform()
   |
   v
4. curl_multi_perform() triggers curl write callback as data arrives
   |
   v
5. Write callback feeds data to SSE parser: ik_sse_parse_chunk(parser, data, len)
   |
   v
6. SSE parser calls ik_anthropic_stream_process_event() for each complete event
   |
   v
7. ik_anthropic_stream_process_event() parses JSON, creates ik_stream_event_t,
   and invokes user's stream_cb(event, stream_ctx)
   |
   v
8. When transfer completes, info_read() detects completion and invokes
   completion_cb(completion, completion_ctx)
```

### Key Async Principle

**NO BLOCKING CALLS.** The functions `ik_http_post_stream()` or any blocking HTTP operation MUST NOT be used. Instead:
- `start_stream()` configures curl and returns immediately
- Actual I/O happens when REPL calls `perform()`
- Events delivered via callbacks invoked from within `perform()`
- Completion signaled via callback from `info_read()`

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_anthropic_stream_ctx_create(TALLOC_CTX *ctx, ik_stream_cb_t stream_cb, void *stream_ctx, ik_anthropic_stream_ctx_t **out)` | Creates streaming context with stream callback only |
| `void ik_anthropic_stream_process_event(ik_anthropic_stream_ctx_t *stream_ctx, const char *event, const char *data)` | Processes single SSE event, emits normalized events via stream callback |
| `ik_usage_t ik_anthropic_stream_get_usage(ik_anthropic_stream_ctx_t *stream_ctx)` | Returns accumulated usage statistics from stream |
| `ik_finish_reason_t ik_anthropic_stream_get_finish_reason(ik_anthropic_stream_ctx_t *stream_ctx)` | Returns finish reason from stream |
| `res_t ik_anthropic_start_stream(void *impl_ctx, const ik_request_t *req, ik_stream_cb_t stream_cb, void *stream_ctx, ik_provider_completion_cb_t completion_cb, void *completion_ctx)` | Vtable start_stream implementation: serialize request, configure curl, add to multi handle, return immediately |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_anthropic_stream_ctx_t` | ctx, stream_cb (ik_stream_cb_t), stream_ctx, sse_parser, model, finish_reason, usage, current_block_index, current_block_type, current_tool_id, current_tool_name | Streaming context tracks state, stream callback, and accumulated metadata. **Note:** Completion callback is passed to start_stream() vtable method, not stored in stream context. This matches Google and OpenAI patterns. |

## Behaviors

### Anthropic SSE Event Types

| Event | Action |
|-------|--------|
| `message_start` | Extract model and initial usage, emit IK_STREAM_START |
| `content_block_start` | Track block type and index, emit IK_STREAM_TOOL_CALL_START for tool_use |
| `content_block_delta` | Emit IK_STREAM_TEXT_DELTA, IK_STREAM_THINKING_DELTA, or IK_STREAM_TOOL_CALL_DELTA based on type |
| `content_block_stop` | Emit IK_STREAM_TOOL_CALL_DONE for tool_use blocks |
| `message_delta` | Update finish_reason and usage, no event emission |
| `message_stop` | Emit IK_STREAM_DONE with accumulated usage and finish_reason |
| `ping` | Ignore (keep-alive) |
| `error` | Parse error type and message, emit IK_STREAM_ERROR |

### Curl Write Callback Integration

The curl write callback (set via `CURLOPT_WRITEFUNCTION`) must:
1. Receive data chunk from network
2. Feed chunk to SSE parser: `ik_sse_parse_chunk(stream_ctx->sse_parser, data, len)`
3. SSE parser invokes `ik_anthropic_stream_process_event()` for each complete event
4. Return `len` to indicate all bytes consumed (or less to abort)

### Streaming Context Creation
- Allocate context with talloc
- Store user stream callback and context
- **Do NOT store completion callback** - it is passed to start_stream() vtable method instead
- Create SSE parser with event callback bound to `ik_anthropic_stream_process_event`
- Initialize current_block_index to -1
- Initialize finish_reason to IK_FINISH_UNKNOWN
- Zero usage statistics

**Note:** Completion callback is passed to start_stream() vtable method, not stored in stream context. This matches Google and OpenAI patterns.

### start_stream() Implementation (NON-BLOCKING)

```
1. Create streaming context with ik_anthropic_stream_ctx_create() (stream callback only)
2. Serialize request with ik_anthropic_serialize_request() (stream: true)
3. Build headers with ik_anthropic_build_headers()
4. Construct URL: base_url + "/v1/messages"
5. Configure curl easy handle:
   - CURLOPT_URL = constructed URL
   - CURLOPT_POST = 1
   - CURLOPT_POSTFIELDS = serialized JSON
   - CURLOPT_HTTPHEADER = headers
   - CURLOPT_WRITEFUNCTION = anthropic_write_callback
   - CURLOPT_WRITEDATA = stream_ctx
   - Store completion_cb/completion_ctx separately (NOT in stream_ctx) for info_read() to invoke
6. Add easy handle to curl_multi via ik_http_multi_add_request()
7. Return OK(NULL) immediately
```

**Critical:** This function MUST return immediately. No blocking I/O. The curl handle is added to multi and actual I/O happens during subsequent perform() calls.

**Note:** The completion callback is handled by start_stream() directly and passed to the curl_multi infrastructure for invocation during info_read(). It is NOT stored in the stream context.

### Message Start Processing
- Extract model name from message object, store in context
- Extract initial input_tokens from usage object
- Emit IK_STREAM_START event with model name via stream_cb

### Content Block Start Processing
- Extract index and content_block.type from event
- For type="text": set current_block_type to IK_CONTENT_TEXT, no event
- For type="thinking": set current_block_type to IK_CONTENT_THINKING, no event
- For type="tool_use": set current_block_type to IK_CONTENT_TOOL_CALL
  - Extract id and name
  - Store in context
  - Emit IK_STREAM_TOOL_CALL_START with id, name, and index

### Content Block Delta Processing
- Extract index and delta.type from event
- For delta.type="text_delta": emit IK_STREAM_TEXT_DELTA with text and index
- For delta.type="thinking_delta": emit IK_STREAM_THINKING_DELTA with thinking and index
- For delta.type="input_json_delta": emit IK_STREAM_TOOL_CALL_DELTA with partial_json and index

### Content Block Stop Processing
- Extract index
- If current_block_type is IK_CONTENT_TOOL_CALL: emit IK_STREAM_TOOL_CALL_DONE with index
- Reset current_block_index to -1

### Message Delta Processing
- Extract delta.stop_reason if present, map using `ik_anthropic_map_finish_reason()`, store in context
- Extract usage.output_tokens and usage.thinking_tokens, accumulate in context
- Update total_tokens calculation
- No event emission

### Message Stop Processing
- Emit IK_STREAM_DONE with finish_reason and accumulated usage via stream_cb

### Error Event Processing
- Parse error.type and error.message from JSON
- Map error.type to category:
  - "authentication_error" -> IK_ERR_CAT_AUTH
  - "rate_limit_error" -> IK_ERR_CAT_RATE_LIMIT
  - "overloaded_error" -> IK_ERR_CAT_SERVER
  - "invalid_request_error" -> IK_ERR_CAT_INVALID_ARG
  - Unknown -> IK_ERR_CAT_UNKNOWN
- Emit IK_STREAM_ERROR with category and message via stream_cb

### Transfer Completion Handling

When curl_multi signals transfer complete (detected in `info_read()`):
1. Retrieve stream_ctx from completed easy handle
2. Receive `ik_http_completion_t` from shared HTTP layer with raw response
3. Parse response_body, extract usage, map errors to `ik_error_category_t`
4. Build `ik_provider_completion_t` with parsed response and metadata
5. Invoke completion_cb(provider_completion, completion_ctx)
6. Clean up curl easy handle (remove from multi, curl_easy_cleanup)

## Test Scenarios

### Simple Text Stream (Async Pattern)
- Call start_stream() - verify returns immediately
- Mock curl_multi to deliver SSE chunks via write callback
- Verify stream_cb receives: IK_STREAM_START, IK_STREAM_TEXT_DELTA (multiple), IK_STREAM_DONE
- Verify completion_cb invoked after final event

### Tool Call Stream (Async Pattern)
- start_stream() returns immediately
- Mock delivers: message_start, content_block_start (tool_use), deltas, stop, message_stop
- Verify stream_cb receives: START, TOOL_CALL_START, TOOL_CALL_DELTA (multiple), TOOL_CALL_DONE, DONE
- Verify completion_cb invoked

### Thinking Stream (Async Pattern)
- Mock delivers content_block_start (type=thinking) and thinking_delta events
- Verify stream_cb receives IK_STREAM_THINKING_DELTA
- Verify thinking text extracted correctly

### Error Stream (Async Pattern)
- Mock delivers error event via write callback
- Verify stream_cb receives IK_STREAM_ERROR with correct category
- Verify completion_cb invoked with error status

### Ping Events
- Mock delivers ping events
- Verify NO events emitted to stream_cb (ping is ignored)

### Usage Accumulation
- input_tokens from message_start
- output_tokens from message_delta
- thinking_tokens from message_delta
- Verify total_tokens calculated correctly in final DONE event

### Non-Blocking Verification
- Call start_stream()
- Verify function returns before any HTTP I/O occurs
- Verify curl easy handle added to multi handle
- Verify no callbacks invoked until perform() is called

## Postconditions

- [ ] `src/providers/anthropic/streaming.h` exists with async interface
- [ ] `src/providers/anthropic/streaming.c` implements async event processing
- [ ] `ik_anthropic_stream_ctx_create()` stores stream callback only (completion callback passed to start_stream() instead)
- [ ] `ik_anthropic_start_stream()` is NON-BLOCKING (adds to curl_multi, returns immediately)
- [ ] Curl write callback feeds SSE parser, which invokes stream_process_event
- [ ] `ik_anthropic_stream_process_event()` handles all SSE event types
- [ ] Text deltas emit IK_STREAM_TEXT_DELTA via stream_cb
- [ ] Thinking deltas emit IK_STREAM_THINKING_DELTA via stream_cb
- [ ] Tool calls emit START, DELTA, DONE events via stream_cb
- [ ] Errors emit IK_STREAM_ERROR with correct category via stream_cb
- [ ] Ping events ignored
- [ ] Usage accumulated correctly from message_start and message_delta
- [ ] Completion callback invoked from info_read() when transfer completes
- [ ] `ik_anthropic_start_stream()` wired to vtable start_stream in anthropic adapter
- [ ] Compiles without warnings
- [ ] All streaming tests pass (using async mock pattern)
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: anthropic-streaming.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
