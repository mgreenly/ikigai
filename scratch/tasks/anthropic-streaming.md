# Task: Anthropic Streaming Implementation

**Layer:** 3
**Model:** sonnet/thinking
**Depends on:** anthropic-response.md, sse-parser.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - Talloc-based memory management

**Source:**
- `src/providers/anthropic/response.h` - Response parsing
- `src/providers/common/sse_parser.h` - SSE parser
- `src/providers/provider.h` - Stream callback types

**Plan:**
- `scratch/plan/configuration.md` - Event normalization and streaming patterns

## Objective

Implement streaming for Anthropic API. Parse Anthropic SSE events and emit normalized `ik_stream_event_t` events through the callback. Handle all Anthropic event types: message_start, content_block_start/delta/stop, message_delta, message_stop, ping, and error.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_anthropic_stream_ctx_create(TALLOC_CTX *ctx, ik_stream_callback_t cb, void *cb_ctx, ik_anthropic_stream_ctx_t **out_stream_ctx)` | Creates streaming context to track state and emit normalized events |
| `void ik_anthropic_stream_process_event(ik_anthropic_stream_ctx_t *stream_ctx, const char *event, const char *data)` | Processes single SSE event, emits normalized events via callback |
| `ik_usage_t ik_anthropic_stream_get_usage(ik_anthropic_stream_ctx_t *stream_ctx)` | Returns accumulated usage statistics from stream |
| `ik_finish_reason_t ik_anthropic_stream_get_finish_reason(ik_anthropic_stream_ctx_t *stream_ctx)` | Returns finish reason from stream |
| `res_t ik_anthropic_stream_impl(void *impl_ctx, ik_request_t *req, ik_stream_callback_t cb, void *cb_ctx)` | Vtable stream implementation: serialize request, POST stream, process events |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_anthropic_stream_ctx_t` | ctx, user_cb, user_ctx, model, finish_reason, usage, current_block_index, current_block_type, current_tool_id, current_tool_name | Streaming context tracks state and accumulated metadata |

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

### Streaming Context Creation
- Allocate context with talloc
- Store user callback and context
- Initialize current_block_index to -1
- Initialize finish_reason to IK_FINISH_UNKNOWN
- Zero usage statistics

### Message Start Processing
- Extract model name from message object, store in context
- Extract initial input_tokens from usage object
- Emit IK_STREAM_START event with model name

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
- Emit IK_STREAM_DONE with finish_reason and accumulated usage

### Error Event Processing
- Parse error.type and error.message from JSON
- Map error.type to category:
  - "authentication_error" → IK_ERR_CAT_AUTH
  - "rate_limit_error" → IK_ERR_CAT_RATE_LIMIT
  - "overloaded_error" → IK_ERR_CAT_SERVER
  - "invalid_request_error" → IK_ERR_CAT_INVALID_ARG
  - Unknown → IK_ERR_CAT_UNKNOWN
- Emit IK_STREAM_ERROR with category and message

### Stream Implementation
- Create streaming context with `ik_anthropic_stream_ctx_create()`
- Serialize request with `ik_anthropic_serialize_request()` (with stream: true flag)
- Build headers with `ik_anthropic_build_headers()`
- Construct URL: base_url + "/v1/messages"
- Create SSE callback bridge that calls `ik_anthropic_stream_process_event()`
- POST stream using `ik_http_post_stream()` with SSE callback
- Return result

## Test Scenarios

### Simple Text Stream
- message_start emits IK_STREAM_START with model
- content_block_start (type=text) does not emit event
- content_block_delta (text_delta) emits IK_STREAM_TEXT_DELTA for each chunk
- content_block_stop does not emit event (text block)
- message_delta updates usage
- message_stop emits IK_STREAM_DONE with finish_reason and usage
- Verify events: START, TEXT_DELTA (multiple), DONE

### Tool Call Stream
- message_start emits START
- content_block_start (type=tool_use) emits IK_STREAM_TOOL_CALL_START with id and name
- content_block_delta (input_json_delta) emits IK_STREAM_TOOL_CALL_DELTA for each chunk
- content_block_stop emits IK_STREAM_TOOL_CALL_DONE
- message_stop emits DONE
- Verify events: START, TOOL_CALL_START, TOOL_CALL_DELTA (multiple), TOOL_CALL_DONE, DONE

### Thinking Stream
- content_block_start (type=thinking) does not emit event
- content_block_delta (thinking_delta) emits IK_STREAM_THINKING_DELTA
- Verify thinking text extracted correctly

### Error Stream
- error event emits IK_STREAM_ERROR
- Error category mapped correctly (e.g., rate_limit_error → IK_ERR_CAT_RATE_LIMIT)
- Error message extracted

### Ping Events
- ping events ignored (no events emitted)

### Usage Accumulation
- input_tokens from message_start
- output_tokens from message_delta
- thinking_tokens from message_delta
- total_tokens calculated correctly

## Postconditions

- [ ] `src/providers/anthropic/streaming.h` exists
- [ ] `src/providers/anthropic/streaming.c` implements event processing
- [ ] `ik_anthropic_stream_ctx_create()` initializes context correctly
- [ ] `ik_anthropic_stream_process_event()` handles all SSE event types
- [ ] Text deltas emit IK_STREAM_TEXT_DELTA
- [ ] Thinking deltas emit IK_STREAM_THINKING_DELTA
- [ ] Tool calls emit START, DELTA, DONE events
- [ ] Errors emit IK_STREAM_ERROR with correct category
- [ ] Ping events ignored
- [ ] Usage accumulated correctly from message_start and message_delta
- [ ] `ik_anthropic_stream_impl()` wired to vtable in anthropic.c
- [ ] Compiles without warnings
- [ ] All streaming tests pass
