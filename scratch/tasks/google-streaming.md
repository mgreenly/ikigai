# Task: Google Streaming Implementation

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** google-response.md, sse-parser.md, http-client.md

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

**Blocking designs are NOT acceptable.** This task implements async streaming.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] `src/providers/google/response.h` exists
- [ ] `src/providers/common/sse_parser.h` exists
- [ ] `src/providers/common/http_multi.h` exists
- [ ] `src/providers/provider.h` exists with async vtable

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - Talloc-based memory management

**Source:**
- `src/providers/google/response.h` - Response parsing
- `src/providers/common/sse_parser.h` - SSE parser
- `src/providers/common/http_multi.h` - Async HTTP with curl_multi
- `src/providers/provider.h` - Async stream callback types and vtable

**Plan:**
- `scratch/plan/streaming.md` - Event normalization and async flow
- `scratch/plan/architecture.md` - Event loop integration
- `scratch/plan/provider-interface.md` - Async vtable specification

## Objective

Implement async streaming for Google Gemini API. Parse Google SSE events and emit normalized `ik_stream_event_t` events through the callback. The implementation integrates with the select()-based event loop via the async vtable pattern (fdset/perform/info_read).

## Key Differences from Anthropic

| Aspect | Anthropic | Google |
|--------|-----------|--------|
| Event types | Many (message_start, content_block_delta, etc.) | Single format (data chunks) |
| Event name | `event: message_start` | No event name, just `data:` |
| Deltas | Explicit delta objects | Full chunks with incremental text |
| Tool call ID | In content_block_start | **Generated on first chunk** (22-char base64url UUID) |
| Thinking | `type: "thinking_delta"` | `thought: true` flag in part |
| Chunk completeness | Fine-grained deltas | Each chunk is complete JSON object |

## Google Streaming Format

- Each chunk is a complete GenerateContentResponse object
- Text accumulates across chunks (not explicit deltas)
- Final chunk includes finishReason and usageMetadata
- No explicit event types (just `data: {JSON}`)
- Each data line contains a complete JSON object
- Thinking content has `thought: true` flag on the part

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_google_stream_ctx_create(TALLOC_CTX *ctx, ik_stream_cb_t cb, void *cb_ctx, ik_google_stream_ctx_t **out_stream_ctx)` | Create streaming context for processing chunks, returns OK/ERR |
| `void ik_google_stream_process_data(ik_google_stream_ctx_t *stream_ctx, const char *data)` | Process single SSE data chunk (JSON string), called from curl write callback |
| `ik_usage_t ik_google_stream_get_usage(ik_google_stream_ctx_t *stream_ctx)` | Get accumulated usage statistics |
| `ik_finish_reason_t ik_google_stream_get_finish_reason(ik_google_stream_ctx_t *stream_ctx)` | Get final finish reason |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_google_stream_ctx_t` | ctx, user_cb, user_ctx, model, finish_reason, usage, started, in_thinking, in_tool_call, current_tool_id, current_tool_name, part_index | Internal streaming state tracking |

## Async Streaming Architecture

The async streaming pattern integrates with the REPL's select()-based event loop:

```
1. REPL calls provider->vt->start_stream(ctx, req, stream_cb, stream_ctx, completion_cb, completion_ctx)
   - Provider serializes request to Google wire format
   - Provider configures curl easy handle with write callback
   - Write callback invokes SSE parser, SSE parser invokes stream_cb
   - Adds easy handle to curl_multi
   - Returns immediately (non-blocking)

2. REPL event loop runs:
   - provider->vt->fdset(ctx, &read_fds, &write_fds, &exc_fds, &max)
   - select(max+1, &read_fds, &write_fds, &exc_fds, &timeout)
   - provider->vt->perform(ctx, &running)  // triggers curl write callbacks
     // -> curl receives data
     // -> write callback invokes SSE parser
     // -> SSE parser invokes ik_google_stream_process_data()
     // -> process_data emits normalized events to stream_cb
   - provider->vt->info_read(ctx, logger)  // invokes completion_cb when done
```

**Key insight:** `ik_google_stream_process_data()` is called from the curl write callback during `perform()`, NOT from a blocking HTTP call.

## Behaviors

**Streaming Context:**
- Create context with user callback and context
- Track accumulated state: model, finish_reason, usage
- Track current content block state: started, in_thinking, in_tool_call
- Generate and store tool call ID on first functionCall chunk (22-char base64url)
- Track part_index for event indexing

**Data Processing (called from curl write callback during perform()):**
- Skip empty data strings
- Parse JSON chunk using yyjson
- Silently ignore malformed JSON chunks
- Check for error object first, emit IK_STREAM_ERROR if present
- Extract modelVersion on first chunk
- Emit IK_STREAM_START on first chunk (if not already emitted)
- Process candidates[0].content.parts[] array

**Part Processing:**
- For functionCall part:
  - If not in_tool_call: generate tool ID (22-char base64url UUID), emit IK_STREAM_TOOL_CALL_START
  - Set in_tool_call=true
  - Extract arguments and emit IK_STREAM_TOOL_CALL_DELTA
- For part with thought=true:
  - End any open tool call first (emit IK_STREAM_TOOL_CALL_DONE)
  - Set in_thinking=true if not already
  - Emit IK_STREAM_THINKING_DELTA with text
- For regular text part:
  - End any open tool call first (emit IK_STREAM_TOOL_CALL_DONE)
  - If transitioning from thinking: increment part_index, set in_thinking=false
  - Emit IK_STREAM_TEXT_DELTA with text
- Skip empty text parts

**Finalization:**
- When usageMetadata present: parse usage, end open tool calls, emit IK_STREAM_DONE
- Extract finishReason from chunk and store
- Usage calculation: output_tokens = candidatesTokenCount - thoughtsTokenCount

**Error Handling:**
- Error object in chunk: extract message and status, map to category, emit IK_STREAM_ERROR
- Map status strings: UNAUTHENTICATED->AUTH, RESOURCE_EXHAUSTED->RATE_LIMIT, INVALID_ARGUMENT->INVALID_ARG

**Integration with Google Adapter (`adapter.c`):**
The google adapter's `start_stream()` implementation:
1. Creates streaming context with `ik_google_stream_ctx_create()`
2. Serializes request using `ik_google_serialize_request()` (with stream flag)
3. Builds URL using `ik_google_build_url()` with streaming=true
4. Builds headers using `ik_google_build_headers()` with streaming=true
5. Creates SSE callback bridge that calls `ik_google_stream_process_data()`
6. Configures curl easy handle with write callback
7. Adds request to `ik_http_multi_t` via `ik_http_multi_add_request()`
8. Returns immediately (request progresses via perform())
9. Google doesn't use event names, SSE callback ignores event parameter

## Test Scenarios

Testing uses the mock curl_multi pattern. Tests feed data to the curl write callback to simulate network activity, then verify events emitted through the stream callback.

**Simple Text Stream:**
- Feed multiple data chunks with text parts via write callback
- Verify IK_STREAM_START emitted first
- Verify IK_STREAM_TEXT_DELTA for each chunk
- Feed final chunk with usageMetadata, verify IK_STREAM_DONE emitted
- Verify finish_reason is STOP

**Thinking Stream:**
- Feed chunk with thought=true part
- Verify IK_STREAM_THINKING_DELTA emitted
- Feed next chunk with regular text, verify IK_STREAM_TEXT_DELTA
- Verify part_index increments on transition

**Function Call Stream:**
- Feed chunk with functionCall part
- Verify IK_STREAM_TOOL_CALL_START emitted with generated 22-char ID
- Verify IK_STREAM_TOOL_CALL_DELTA emitted with arguments
- Feed final chunk, verify IK_STREAM_TOOL_CALL_DONE then IK_STREAM_DONE

**Error Stream:**
- Feed chunk with error object
- Verify IK_STREAM_ERROR emitted
- Category maps correctly (RESOURCE_EXHAUSTED -> RATE_LIMIT)

**Empty Data:**
- Empty string is ignored (no events)
- Invalid JSON is ignored (no events)

**Usage Accumulation:**
- Final chunk with usageMetadata extracts all token counts
- thinking_tokens from thoughtsTokenCount
- output_tokens = candidatesTokenCount - thoughtsTokenCount
- Verify get_usage() returns correct values

**Model Extraction:**
- First chunk with modelVersion stores in context
- IK_STREAM_START event includes model name

**Callback Invocation Context:**
- Verify stream_cb is called during data processing (simulates perform())
- Verify events are delivered synchronously to stream_cb
- Verify no events after IK_STREAM_DONE or IK_STREAM_ERROR

## Postconditions

**File Structure:**
- [ ] `src/providers/google/streaming.h` exists with function declarations
- [ ] `src/providers/google/streaming.c` implements event processing
- [ ] Makefile updated with streaming.c

**Core Functionality:**
- [ ] `ik_google_stream_ctx_create()` creates streaming context
- [ ] `ik_google_stream_process_data()` handles all chunk types
- [ ] Text chunks emit `IK_STREAM_TEXT_DELTA`
- [ ] Thinking chunks (with `thought: true`) emit `IK_STREAM_THINKING_DELTA`
- [ ] Function calls emit START, DELTA, DONE events with generated 22-char base64url ID
- [ ] Errors emit `IK_STREAM_ERROR` with correct category
- [ ] Final chunk with `usageMetadata` triggers `IK_STREAM_DONE`
- [ ] Empty/malformed data is silently ignored

**Async Integration:**
- [ ] `ik_google_stream_process_data()` is called from curl write callback
- [ ] Events are emitted synchronously during data processing
- [ ] No blocking calls in the streaming code path
- [ ] Streaming context lifetime managed correctly (freed after completion)

**Tests:**
- [ ] All streaming tests pass using mock curl_multi pattern
- [ ] Tests verify events through captured callback invocations

**Git:**
- [ ] Changes committed to git with message: `task: google-streaming.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).