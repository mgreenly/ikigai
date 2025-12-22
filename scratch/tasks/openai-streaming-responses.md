# Task: OpenAI Responses API Streaming

**Model:** sonnet/thinking
**Depends on:** openai-response-responses.md, openai-streaming-chat.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/openai/streaming.h` - Streaming header (from openai-streaming-chat.md)
- `src/providers/openai/response.h` - Status mapping
- `src/providers/provider.h` - Stream callback types

**Plan:**
- `scratch/plan/streaming.md` - Event normalization

## Objective

Implement streaming for OpenAI Responses API. Handle SSE events with semantic event names like `response.output_text.delta` and `response.completed`. Normalize to internal streaming events.

## Responses API Stream Format

Event-based SSE stream with semantic names:
```
event: response.created
data: {"type":"response.created","id":"resp_123","model":"o3"}

event: response.output_text.delta
data: {"type":"response.output_text.delta","delta":"Hello"}

event: response.reasoning_summary_text.delta
data: {"type":"response.reasoning_summary_text.delta","delta":"Let me think..."}

event: response.completed
data: {"type":"response.completed","status":"completed","usage":{...}}
```

Key characteristics:
- Uses `event:` field with semantic names
- `response.output_text.delta` for text output
- `response.reasoning_summary_text.delta` for thinking summary
- `response.function_call_arguments.delta` for tool arguments
- `response.completed` terminates stream

## Interface

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_responses_stream_ctx_t` | ctx, user_cb, user_ctx, model, finish_reason, usage, started, in_tool_call, tool_call_index, current_tool_id, current_tool_name | Responses API streaming state |

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_responses_stream_ctx_create(ctx, cb, cb_ctx, out_stream_ctx)` | Create streaming context for Responses API |
| `ik_openai_responses_stream_process_event(stream_ctx, event, data)` | Process SSE event with name and data |
| `ik_openai_responses_stream_get_usage(stream_ctx)` | Get accumulated usage from stream |
| `ik_openai_responses_stream_get_finish_reason(stream_ctx)` | Get finish reason from stream |

### Helper Functions (Internal)

- `emit_event(stream, event)` - Call user callback with event
- `maybe_emit_start(stream)` - Emit START if not yet started
- `maybe_end_tool_call(stream)` - End current tool call if active
- `parse_usage(usage_val, out_usage)` - Extract usage from JSON

## Behaviors

### Stream Initialization

- Create stream context with user callback
- Initialize finish_reason to UNKNOWN
- Set started = false, in_tool_call = false
- Initialize tool_call_index = -1

### Event Processing

- Parse JSON data for all events
- Dispatch based on event name
- Handle events: response.created, response.in_progress, response.output_text.delta, response.reasoning_summary_text.delta, response.output_item.added, response.function_call_arguments.delta, response.function_call_arguments.done, response.completed, error

### response.created

- Extract model name
- Emit START event
- Set started = true

### response.output_text.delta

- Extract delta field
- Emit IK_STREAM_TEXT_DELTA with text and index=0
- Ensure START emitted first

### response.reasoning_summary_text.delta

- Extract delta field
- Emit IK_STREAM_THINKING_DELTA with text
- This is unique to Responses API (thinking summary)

### response.output_item.added

- Check type field for "function_call"
- Extract item.call_id and item.name
- Extract output_index
- End previous tool call if active
- Emit IK_STREAM_TOOL_CALL_START
- Set in_tool_call = true

### response.function_call_arguments.delta

- Extract delta field
- Extract output_index
- Emit IK_STREAM_TOOL_CALL_DELTA with arguments chunk

### response.function_call_arguments.done

- End current tool call
- Emit IK_STREAM_TOOL_CALL_DONE

### response.completed

- End any active tool call
- Extract status field
- Extract incomplete_details.reason if status is "incomplete"
- Map to finish reason using `ik_openai_map_responses_status()`
- Extract usage from response.usage or root usage
- Emit IK_STREAM_DONE with finish_reason and usage

### error Event

- Extract error.message and error.type
- Map type to category (authentication_error, rate_limit_error)
- Emit IK_STREAM_ERROR with category and message

### Usage Extraction

- Check response.usage first, fallback to root usage
- Extract prompt_tokens, completion_tokens, total_tokens
- Extract completion_tokens_details.reasoning_tokens for thinking tokens

### Finish Reason Mapping

- Use `ik_openai_map_responses_status()` for status + incomplete_reason
- Store in stream context
- Include in DONE event

## Test Scenarios

### Simple Text Stream

- response.created event
- Series of output_text.delta events
- response.completed event
- Verify START, TEXT_DELTA, TEXT_DELTA, DONE sequence

### Reasoning Summary Stream

- response.reasoning_summary_text.delta events
- Verify IK_STREAM_THINKING_DELTA emitted
- Verify thinking text included

### Function Call Stream

- response.output_item.added with function_call type
- response.function_call_arguments.delta events
- response.function_call_arguments.done event
- Verify TOOL_CALL_START, DELTA, DELTA, DONE sequence

### Mixed Content Stream

- Reasoning summary deltas
- Output text deltas
- Verify both thinking and text events emitted

### Error Event

- error event with message and type
- Verify ERROR event emitted
- Verify category mapping

### Incomplete Status

- response.completed with status="incomplete"
- incomplete_details.reason="max_output_tokens"
- Verify finish_reason = LENGTH

### Usage Extraction

- response.completed with usage and reasoning_tokens
- Verify usage extracted correctly
- Verify thinking_tokens populated

## Postconditions

- [ ] `src/providers/openai/streaming.h` declares Responses streaming types and functions
- [ ] `src/providers/openai/streaming_responses.c` implements event processing
- [ ] response.created emits START event
- [ ] response.output_text.delta emits TEXT_DELTA
- [ ] response.reasoning_summary_text.delta emits THINKING_DELTA
- [ ] response.function_call_arguments.delta emits TOOL_CALL_DELTA
- [ ] response.completed emits DONE with status mapping
- [ ] error event emits STREAM_ERROR
- [ ] Usage extracted from completed event
- [ ] Makefile updated with streaming_responses.c
- [ ] All tests pass
- [ ] Compiles without warnings
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: openai-streaming-responses.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).