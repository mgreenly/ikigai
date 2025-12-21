# Task: OpenAI Chat Completions Streaming

**Layer:** 4
**Model:** sonnet/thinking
**Depends on:** openai-response-chat.md, sse-parser.md

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/openai/response.h` - Finish reason mapping
- `src/providers/common/sse_parser.h` - SSE parser
- `src/providers/provider.h` - Stream callback types
- `src/providers/anthropic/streaming.c` - Example streaming pattern

**Plan:**
- `scratch/plan/streaming.md` - Event normalization

## Objective

Implement streaming for OpenAI Chat Completions API. Handle SSE data events with `choices[].delta` format and `[DONE]` terminator. Normalize to internal streaming events.

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

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_chat_stream_ctx_t` | ctx, user_cb, user_ctx, model, finish_reason, usage, started, in_tool_call, tool_call_index, current_tool_id, current_tool_name | Chat Completions streaming state |

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_chat_stream_ctx_create(ctx, cb, cb_ctx, out_stream_ctx)` | Create streaming context for Chat Completions API |
| `ik_openai_chat_stream_process_data(stream_ctx, data)` | Process SSE data line (JSON or "[DONE]") |
| `ik_openai_chat_stream_get_usage(stream_ctx)` | Get accumulated usage from stream |
| `ik_openai_chat_stream_get_finish_reason(stream_ctx)` | Get finish reason from stream |

### Helper Functions (Internal)

- `emit_event(stream, event)` - Call user callback with event
- `maybe_emit_start(stream)` - Emit START if not yet started
- `maybe_end_tool_call(stream)` - End current tool call if active
- `process_delta(stream, delta, finish_reason)` - Process choices[0].delta

## Behaviors

### Stream Initialization

- Create stream context with user callback
- Initialize finish_reason to UNKNOWN
- Set started = false, in_tool_call = false
- Initialize tool_call_index = -1

### START Event

- Emit once when first delta arrives
- Include model name from first chunk
- Set started = true after emitting

### Data Processing

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
  - End previous tool call if active
  - Start new tool call (emit START)
  - Set in_tool_call = true
- When arguments delta arrives:
  - Emit DELTA with arguments chunk
- When tool call ends (new index or stream done):
  - Emit DONE
  - Set in_tool_call = false

### Usage Extraction

- Usage included in final chunk with stream_options.include_usage
- Extract prompt_tokens, completion_tokens, total_tokens
- Check completion_tokens_details.reasoning_tokens for thinking tokens
- Store in stream context

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

## Test Scenarios

### Simple Text Stream

- Series of content deltas
- Verify START emitted once
- Verify TEXT_DELTA for each chunk
- Verify DONE emitted with finish_reason

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

## Postconditions

- [ ] `src/providers/openai/streaming.h` declares Chat streaming types and functions
- [ ] `src/providers/openai/streaming_chat.c` implements Chat streaming
- [ ] Handles data-only SSE format (no event names)
- [ ] "[DONE]" marker terminates stream correctly
- [ ] Text deltas emit IK_STREAM_TEXT_DELTA events
- [ ] Tool calls emit START, DELTA, DONE events
- [ ] Errors emit IK_STREAM_ERROR with correct category
- [ ] Usage extracted from final chunk
- [ ] Makefile updated with streaming_chat.c
- [ ] All streaming tests pass
- [ ] Compiles without warnings
- [ ] `make check` passes
