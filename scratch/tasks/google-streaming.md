# Task: Google Streaming Implementation

**Layer:** 3
**Model:** sonnet/thinking
**Depends on:** google-response.md, sse-parser.md

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/providers/google/response.h` - Response parsing
- `src/providers/common/sse_parser.h` - SSE parser
- `src/providers/provider.h` - Stream callback types

**Plan:**
- `scratch/plan/streaming.md` - Event normalization

## Objective

Implement streaming for Google Gemini API. Parse Google SSE events and emit normalized `ik_stream_event_t` events through the callback.

## Key Differences from Anthropic

| Aspect | Anthropic | Google |
|--------|-----------|--------|
| Event types | Many (message_start, content_block_delta, etc.) | Single format (data chunks) |
| Event name | `event: message_start` | No event name, just `data:` |
| Deltas | Explicit delta objects | Full chunks with incremental text |
| Tool call ID | In content_block_start | **Generated on first chunk** |
| Thinking | `type: "thinking_delta"` | `thought: true` flag |

## Google Streaming Format

- Each chunk is a complete GenerateContentResponse object
- Text accumulates across chunks (not explicit deltas)
- Final chunk includes finishReason and usageMetadata
- No explicit event types (just data: {JSON})
- Each data line contains a complete JSON object

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_google_stream_ctx_create(TALLOC_CTX *ctx, ik_stream_callback_t cb, void *cb_ctx, ik_google_stream_ctx_t **out_stream_ctx)` | Create streaming context for processing chunks, returns OK/ERR |
| `void ik_google_stream_process_data(ik_google_stream_ctx_t *stream_ctx, const char *data)` | Process single SSE data chunk (JSON string) |
| `ik_usage_t ik_google_stream_get_usage(ik_google_stream_ctx_t *stream_ctx)` | Get accumulated usage statistics |
| `ik_finish_reason_t ik_google_stream_get_finish_reason(ik_google_stream_ctx_t *stream_ctx)` | Get final finish reason |
| `res_t ik_google_stream_impl(void *impl_ctx, ik_request_t *req, ik_stream_callback_t cb, void *cb_ctx)` | Vtable stream implementation |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_google_stream_ctx_t` | ctx, user_cb, user_ctx, model, finish_reason, usage, started, in_thinking, in_tool_call, current_tool_id, current_tool_name, part_index | Internal streaming state tracking |

## Behaviors

**Streaming Context:**
- Create context with user callback and context
- Track accumulated state: model, finish_reason, usage
- Track current content block state: started, in_thinking, in_tool_call
- Generate and store tool call ID on first functionCall chunk
- Track part_index for event indexing

**Data Processing:**
- Skip empty data strings
- Parse JSON chunk using yyjson
- Silently ignore malformed JSON chunks
- Check for error object first, emit IK_STREAM_ERROR if present
- Extract modelVersion on first chunk
- Emit IK_STREAM_START on first chunk (if not already emitted)
- Process candidates[0].content.parts[] array

**Part Processing:**
- For functionCall part:
  - If not in_tool_call: generate tool ID, emit IK_STREAM_TOOL_CALL_START
  - Set in_tool_call=true
  - Extract arguments and emit IK_STREAM_TOOL_CALL_DELTA
- For part with thought=true:
  - End any open tool call first
  - Set in_thinking=true if not already
  - Emit IK_STREAM_THINKING_DELTA with text
- For regular text part:
  - End any open tool call first
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

**Stream Implementation:**
- Create streaming context with user callback
- Serialize request using ik_google_serialize_request()
- Build URL using ik_google_build_url() with streaming=true
- Build headers using ik_google_build_headers() with streaming=true
- Create SSE callback bridge to call ik_google_stream_process_data()
- Make streaming HTTP POST using ik_http_post_stream()
- Google doesn't use event names, SSE callback ignores event parameter

## Test Scenarios

**Simple Text Stream:**
- Process multiple data chunks with text parts
- Verify IK_STREAM_START emitted first
- Verify IK_STREAM_TEXT_DELTA for each chunk
- Final chunk with usageMetadata emits IK_STREAM_DONE
- Verify finish_reason is STOP

**Thinking Stream:**
- Process chunk with thought=true part
- Verify IK_STREAM_THINKING_DELTA emitted
- Next chunk with regular text emits IK_STREAM_TEXT_DELTA
- Verify part_index increments on transition

**Function Call Stream:**
- Process chunk with functionCall part
- Verify IK_STREAM_TOOL_CALL_START emitted with generated 22-char ID
- Verify IK_STREAM_TOOL_CALL_DELTA emitted with arguments
- Final chunk emits IK_STREAM_TOOL_CALL_DONE then IK_STREAM_DONE

**Error Stream:**
- Process chunk with error object
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

## Postconditions

- [ ] `src/providers/google/streaming.h` exists
- [ ] `src/providers/google/streaming.c` implements event processing
- [ ] `ik_google_stream_process_data()` handles all chunk types
- [ ] Text chunks emit `IK_STREAM_TEXT_DELTA`
- [ ] Thinking chunks (with `thought: true`) emit `IK_STREAM_THINKING_DELTA`
- [ ] Function calls emit START, DELTA, DONE events with generated ID
- [ ] Errors emit `IK_STREAM_ERROR` with correct category
- [ ] Final chunk with `usageMetadata` triggers `IK_STREAM_DONE`
- [ ] Empty/malformed data is silently ignored
- [ ] `ik_google_stream_impl()` wired to vtable in google.c
- [ ] Makefile updated with streaming.c
- [ ] All streaming tests pass
