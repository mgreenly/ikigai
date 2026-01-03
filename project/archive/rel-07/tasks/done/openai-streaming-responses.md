# Task: OpenAI Responses API Streaming

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet/thinking
**Depends on:** openai-response-responses.md, openai-streaming-chat.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL streaming operations MUST be non-blocking:

- Use curl_multi (NOT curl_easy)
- `start_stream()` returns immediately after adding request to curl_multi
- SSE parsing happens in curl write callback during `perform()`
- Stream events delivered to user callback during `perform()`
- Completion callback invoked from `info_read()` when transfer completes
- NEVER block the main thread

Reference: `scratch/plan/README.md`, `scratch/plan/architecture.md`

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/openai/streaming.h` - Streaming header (from openai-streaming-chat.md)
- `src/providers/openai/response.h` - Status mapping
- `src/providers/provider.h` - Stream callback types, async vtable definition
- `src/providers/common/http_multi.h` - curl_multi wrapper for async HTTP
- `src/providers/common/sse_parser.h` - SSE parser invoked from write callback

**Plan:**
- `scratch/plan/streaming.md` - Async streaming data flow, event normalization
- `scratch/plan/provider-interface.md` - Vtable specification with start_stream()
- `scratch/plan/architecture.md` - Event loop integration pattern

## Objective

Implement async streaming for OpenAI Responses API. Handle SSE events with semantic event names like `response.created`, `response.output_text.delta`, `response.reasoning_summary_text.delta`, and `response.completed`. Normalize to internal streaming events via callbacks invoked during `perform()` in the event loop.

## Responses API Stream Format

Event-based SSE stream with semantic names:
```
event: response.created
data: {"type":"response.created","response":{"id":"resp_123","model":"o3",...}}

event: response.in_progress
data: {"type":"response.in_progress","response":{...}}

event: response.output_item.added
data: {"type":"response.output_item.added","output_index":0,"item":{"type":"message","id":"item_123"}}

event: response.content_part.added
data: {"type":"response.content_part.added","item_id":"item_123","content_index":0,"part":{"type":"output_text","text":""}}

event: response.output_text.delta
data: {"type":"response.output_text.delta","item_id":"item_123","content_index":0,"delta":"Hello"}

event: response.reasoning_summary_text.delta
data: {"type":"response.reasoning_summary_text.delta","item_id":"item_123","summary_index":0,"delta":"Let me think..."}

event: response.output_item.added
data: {"type":"response.output_item.added","output_index":1,"item":{"type":"function_call","call_id":"call_123","name":"bash"}}

event: response.function_call_arguments.delta
data: {"type":"response.function_call_arguments.delta","item_id":"item_456","output_index":1,"delta":"{\"cmd\":"}

event: response.function_call_arguments.done
data: {"type":"response.function_call_arguments.done","item_id":"item_456","output_index":1,"arguments":"{\"cmd\":\"ls\"}"}

event: response.output_item.done
data: {"type":"response.output_item.done","output_index":1,"item":{...}}

event: response.completed
data: {"type":"response.completed","response":{"status":"completed","usage":{"input_tokens":10,"output_tokens":50,"output_tokens_details":{"reasoning_tokens":20}}}}

event: error
data: {"type":"error","error":{"type":"rate_limit_error","message":"Rate limit exceeded"}}
```

Key characteristics:
- Uses `event:` field with semantic names (unlike Chat Completions API)
- `response.created` marks stream start with model info
- `response.output_text.delta` for text output (includes item_id and content_index)
- `response.reasoning_summary_text.delta` for thinking summary (unique to Responses API)
- `response.output_item.added` with `type:"function_call"` starts tool calls
- `response.function_call_arguments.delta` for tool argument fragments
- `response.function_call_arguments.done` marks tool call arguments complete
- `response.output_item.done` marks tool call fully complete
- `response.completed` terminates stream with status and usage

## Interface

### Async Architecture Overview

The streaming implementation integrates with the select()-based event loop:

```
REPL Event Loop                        Provider/Streaming
===============                        ==================

1. provider->vt->start_stream()  --->  Configure curl easy handle
   (returns immediately)               Add to curl_multi
                                       Register write callback

2. provider->vt->fdset()         --->  curl_multi_fdset()
   (populate fd_sets for select)

3. select() blocks waiting

4. provider->vt->perform()       --->  curl_multi_perform()
   (data arrives, triggers             curl write callback invoked
    write callbacks)                   SSE parser processes chunks
                                       stream_cb() called with events
                                       UI updates in callback

5. provider->vt->info_read()     --->  curl_multi_info_read()
   (check for completion)              If CURLMSG_DONE:
                                         completion_cb() invoked
```

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_responses_stream_ctx_t` | parent (talloc), stream_cb, stream_ctx, model, finish_reason, usage, started, in_tool_call, tool_call_index, current_tool_id, current_tool_name, sse_parser | Responses API streaming state with stream callback only (completion callback passed to start_stream() vtable method) |

**Note:** Following the canonical pattern from `anthropic-streaming.md`, the stream context stores ONLY `stream_cb` and `stream_ctx`. The completion callback (`completion_cb`, `completion_ctx`) is passed to the `start_stream()` vtable method, not stored in the stream context.

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_responses_stream_ctx_create(ctx, stream_cb, stream_ctx, out_stream_ctx)` | Create streaming context with stream callback only (completion callback passed to start_stream() instead) |
| `ik_openai_responses_stream_process_event(stream_ctx, event_name, data)` | Process SSE event (called from curl write callback during perform()) |
| `ik_openai_responses_stream_get_usage(stream_ctx)` | Get accumulated usage from stream |
| `ik_openai_responses_stream_get_finish_reason(stream_ctx)` | Get finish reason from stream |
| `ik_openai_responses_stream_write_callback(ptr, size, nmemb, userdata)` | Curl write callback - feeds data to SSE parser |

**Note:** The `start_stream()` vtable method receives completion callback parameters and stores them separately for invocation during `info_read()`, following the canonical pattern from `anthropic-streaming.md` and `repl-streaming-updates.md`.

### Write Callback Flow

The curl write callback is invoked during `perform()`:

```c
// Registered when request is added to curl_multi
curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, ik_openai_responses_stream_write_callback);
curl_easy_setopt(easy, CURLOPT_WRITEDATA, stream_ctx);

// Called during perform() as data arrives
static size_t ik_openai_responses_stream_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    ik_openai_responses_stream_ctx_t *ctx = userdata;
    size_t total = size * nmemb;

    // Feed to SSE parser - this invokes process_event for each complete event
    ik_sse_parse_chunk(ctx->sse_parser, ptr, total);

    return total;  // Return bytes consumed
}
```

### Helper Functions (Internal)

- `emit_event(stream, event)` - Invoke stream_cb with normalized event
- `maybe_emit_start(stream)` - Emit IK_STREAM_START if not yet started
- `maybe_end_tool_call(stream)` - Emit IK_STREAM_TOOL_CALL_DONE if tool active
- `parse_usage(usage_val, out_usage)` - Extract usage from JSON
- `sse_event_handler(event_name, data, len, user_ctx)` - SSE parser callback that routes to process_event

## Behaviors

**Canonical Callback Ownership Pattern:**
This implementation follows the canonical pattern documented in `anthropic-streaming.md` and `repl-streaming-updates.md`:
- Stream context stores ONLY `stream_cb` and `stream_ctx` (for stream events during perform())
- Completion callback (`completion_cb`, `completion_ctx`) is passed to `start_stream()` vtable method
- `start_stream()` stores completion callback separately for invocation during `info_read()`
- Stream context does NOT store completion callbacks

### Stream Initialization (in create function)

- Create stream context with stream_cb and stream_ctx only
- Create SSE parser with sse_event_handler callback pointing to this context
- Initialize finish_reason to UNKNOWN
- Set started = false, in_tool_call = false
- Initialize tool_call_index = -1
- **Does NOT start any I/O** - that happens when request is added to curl_multi
- **Does NOT store completion callback** - that is passed to `start_stream()` vtable method instead

### Event Processing (invoked during perform() via write callback)

The flow is:
1. `perform()` triggers curl I/O
2. Data arrives, curl invokes write callback
3. Write callback feeds data to SSE parser
4. SSE parser invokes `sse_event_handler` for each complete event
5. `sse_event_handler` calls `process_event` with event name and data
6. `process_event` parses JSON and emits normalized events via stream_cb

Events to handle:
- response.created - Stream started
- response.in_progress - Stream in progress (can ignore or log)
- response.output_item.added - New output item (text or function_call)
- response.content_part.added - New content part (can ignore)
- response.output_text.delta - Text delta
- response.reasoning_summary_text.delta - Thinking summary delta
- response.function_call_arguments.delta - Tool argument fragments
- response.function_call_arguments.done - Tool arguments complete
- response.output_item.done - Output item complete
- response.completed - Stream finished
- error - Error occurred

### response.created

- Parse JSON: `{"type":"response.created","response":{"id":"...","model":"o3",...}}`
- Extract model from `response.model`
- Emit IK_STREAM_START via stream_cb
- Set started = true

### response.output_text.delta

- Parse JSON: `{"type":"response.output_text.delta","item_id":"...","content_index":0,"delta":"Hello"}`
- Extract `delta` field (the text fragment)
- Extract `content_index` for multi-block tracking
- Ensure START was emitted (call maybe_emit_start)
- Emit IK_STREAM_TEXT_DELTA via stream_cb with text and index

### response.reasoning_summary_text.delta

- Parse JSON: `{"type":"response.reasoning_summary_text.delta","item_id":"...","summary_index":0,"delta":"Let me think..."}`
- Extract `delta` field (thinking text fragment)
- Extract `summary_index` for multi-block tracking
- Emit IK_STREAM_THINKING_DELTA via stream_cb
- This is unique to Responses API (o1/o3 reasoning summary)

### response.output_item.added

- Parse JSON: `{"type":"response.output_item.added","output_index":1,"item":{"type":"function_call","call_id":"call_123","name":"bash"}}`
- Check `item.type` field
- If type is "function_call":
  - Extract `item.call_id` and `item.name`
  - Extract `output_index`
  - End previous tool call if active (call maybe_end_tool_call)
  - Set current_tool_id = item.call_id
  - Set current_tool_name = item.name
  - Set tool_call_index = output_index
  - Emit IK_STREAM_TOOL_CALL_START via stream_cb with id, name, index
  - Set in_tool_call = true
- If type is "message": ignore (text will come via output_text.delta)

### response.function_call_arguments.delta

- Parse JSON: `{"type":"response.function_call_arguments.delta","item_id":"...","output_index":1,"delta":"{\"cmd\":"}`
- Extract `delta` field (JSON fragment)
- Extract `output_index`
- Emit IK_STREAM_TOOL_CALL_DELTA via stream_cb with arguments chunk and index

### response.function_call_arguments.done

- Parse JSON: `{"type":"response.function_call_arguments.done","item_id":"...","output_index":1,"arguments":"{\"cmd\":\"ls\"}"}`
- Can use this to verify accumulated arguments match
- Note: IK_STREAM_TOOL_CALL_DONE emitted on response.output_item.done

### response.output_item.done

- Parse JSON: `{"type":"response.output_item.done","output_index":1,"item":{...}}`
- Check if this completes a tool call (in_tool_call and matching index)
- If tool call:
  - Emit IK_STREAM_TOOL_CALL_DONE via stream_cb with index
  - Set in_tool_call = false
  - Clear current_tool_id, current_tool_name

### response.completed

- Parse JSON: `{"type":"response.completed","response":{"status":"completed","usage":{...}}}`
- End any active tool call (call maybe_end_tool_call)
- Extract `response.status` field
- If status is "incomplete":
  - Extract `response.incomplete_details.reason` (e.g., "max_output_tokens")
- Map to finish reason using `ik_openai_map_responses_status(status, incomplete_reason)`
- Extract usage from `response.usage`:
  - `input_tokens` -> usage.prompt_tokens
  - `output_tokens` -> usage.completion_tokens
  - `total_tokens` -> usage.total_tokens (or sum if not present)
  - `output_tokens_details.reasoning_tokens` -> usage.thinking_tokens
- Emit IK_STREAM_DONE via stream_cb with finish_reason and usage
- **Note:** completion_cb is NOT invoked here - it's invoked by `info_read()` when curl signals transfer complete. The completion callback is handled by the `start_stream()` implementation and curl_multi infrastructure.

### error Event

- Parse JSON: `{"type":"error","error":{"type":"rate_limit_error","message":"Rate limit exceeded"}}`
- Extract `error.message` and `error.type`
- Map type to category:
  - "authentication_error" -> IK_ERR_CAT_AUTH
  - "rate_limit_error" -> IK_ERR_CAT_RATE_LIMIT
  - "invalid_request_error" -> IK_ERR_CAT_INVALID_ARG
  - "server_error" -> IK_ERR_CAT_SERVER
  - default -> IK_ERR_CAT_UNKNOWN
- Emit IK_STREAM_ERROR via stream_cb with category and message
- **Note:** completion_cb will be invoked from `info_read()` when curl detects connection close. The completion callback is NOT stored in stream context - it's handled by the `start_stream()` vtable method.

### Usage Extraction

- Responses API provides usage in `response.completed` event
- Path: `response.usage.input_tokens`, `response.usage.output_tokens`
- Reasoning tokens: `response.usage.output_tokens_details.reasoning_tokens`
- These map to internal usage structure for token tracking

### Finish Reason Mapping (ik_openai_map_responses_status)

Responses API uses `status` field with different values than Chat Completions:

| status | incomplete_reason | Maps To |
|--------|-------------------|---------|
| "completed" | - | STOP |
| "incomplete" | "max_output_tokens" | LENGTH |
| "incomplete" | "content_filter" | CONTENT_FILTER |
| "failed" | - | ERROR |
| "cancelled" | - | CANCELLED |

Store finish_reason in stream context for DONE event

## Test Scenarios

Tests simulate the async flow by invoking process_event directly (as if called from write callback during perform). The test captures events emitted to stream_cb.

### Simple Text Stream

Input events (simulating SSE feed during perform):
1. `event: response.created` with model "o3"
2. `event: response.output_text.delta` with delta "Hello"
3. `event: response.output_text.delta` with delta " world"
4. `event: response.completed` with status "completed"

Expected stream_cb invocations:
1. IK_STREAM_START with model="o3"
2. IK_STREAM_TEXT_DELTA with text="Hello", index=0
3. IK_STREAM_TEXT_DELTA with text=" world", index=0
4. IK_STREAM_DONE with finish_reason=STOP, usage populated

### Reasoning Summary Stream

Input events:
1. `event: response.created`
2. `event: response.reasoning_summary_text.delta` with delta "Let me think..."
3. `event: response.reasoning_summary_text.delta` with delta " about this problem."
4. `event: response.output_text.delta` with delta "Here is my answer."
5. `event: response.completed`

Expected stream_cb invocations:
1. IK_STREAM_START
2. IK_STREAM_THINKING_DELTA with text="Let me think..."
3. IK_STREAM_THINKING_DELTA with text=" about this problem."
4. IK_STREAM_TEXT_DELTA with text="Here is my answer."
5. IK_STREAM_DONE

### Function Call Stream

Input events:
1. `event: response.created`
2. `event: response.output_item.added` with type="function_call", call_id="call_123", name="bash"
3. `event: response.function_call_arguments.delta` with delta="{\"cmd\":"
4. `event: response.function_call_arguments.delta` with delta="\"ls\"}"
5. `event: response.function_call_arguments.done` with arguments="{\"cmd\":\"ls\"}"
6. `event: response.output_item.done` with output_index matching tool
7. `event: response.completed`

Expected stream_cb invocations:
1. IK_STREAM_START
2. IK_STREAM_TOOL_CALL_START with id="call_123", name="bash", index=1
3. IK_STREAM_TOOL_CALL_DELTA with arguments="{\"cmd\":", index=1
4. IK_STREAM_TOOL_CALL_DELTA with arguments="\"ls\"}", index=1
5. IK_STREAM_TOOL_CALL_DONE with index=1
6. IK_STREAM_DONE

### Multiple Tool Calls

Input events:
1. `event: response.created`
2. `event: response.output_item.added` with type="function_call", name="bash", output_index=0
3. `event: response.function_call_arguments.delta` (tool 0)
4. `event: response.output_item.done` (tool 0)
5. `event: response.output_item.added` with type="function_call", name="file_read", output_index=1
6. `event: response.function_call_arguments.delta` (tool 1)
7. `event: response.output_item.done` (tool 1)
8. `event: response.completed`

Expected:
- Two IK_STREAM_TOOL_CALL_START events with different indices
- Tool 0 completes before tool 1 starts

### Mixed Content Stream (Thinking + Text + Tool)

Input events:
1. `event: response.created`
2. `event: response.reasoning_summary_text.delta` with thinking text
3. `event: response.output_text.delta` with text
4. `event: response.output_item.added` with function_call
5. `event: response.function_call_arguments.delta`
6. `event: response.output_item.done`
7. `event: response.completed`

Expected: Interleaved THINKING_DELTA, TEXT_DELTA, and TOOL_CALL events

### Error Event

Input events:
1. `event: response.created`
2. `event: error` with type="rate_limit_error", message="Rate limit exceeded"

Expected stream_cb invocations:
1. IK_STREAM_START
2. IK_STREAM_ERROR with category=IK_ERR_CAT_RATE_LIMIT, message="Rate limit exceeded"

Verify error category mapping:
- "authentication_error" -> IK_ERR_CAT_AUTH
- "rate_limit_error" -> IK_ERR_CAT_RATE_LIMIT
- "invalid_request_error" -> IK_ERR_CAT_INVALID_ARG

### Incomplete Status (Length)

Input events:
1. `event: response.created`
2. `event: response.output_text.delta` with partial text
3. `event: response.completed` with status="incomplete", incomplete_details.reason="max_output_tokens"

Expected:
- IK_STREAM_DONE with finish_reason=LENGTH (not STOP)

### Incomplete Status (Content Filter)

Input events:
1. `event: response.completed` with status="incomplete", incomplete_details.reason="content_filter"

Expected:
- IK_STREAM_DONE with finish_reason=CONTENT_FILTER

### Usage Extraction

Input event:
```json
{
  "type": "response.completed",
  "response": {
    "status": "completed",
    "usage": {
      "input_tokens": 100,
      "output_tokens": 250,
      "total_tokens": 350,
      "output_tokens_details": {
        "reasoning_tokens": 50
      }
    }
  }
}
```

Expected:
- usage.prompt_tokens = 100
- usage.completion_tokens = 250
- usage.total_tokens = 350
- usage.thinking_tokens = 50

### Async Integration Test

Simulate full async flow:
1. Create stream context with mock stream_cb only (completion_cb passed to start_stream() separately)
2. Create mock SSE data chunks (may be partial events)
3. Feed chunks via write_callback (simulating curl write callback during perform)
4. Verify stream_cb receives correct sequence of normalized events
5. Verify completion_cb is not called from write_callback (it comes from info_read)
6. Verify stream context does not store completion callback (only stream callback)

## Postconditions

- [ ] `src/providers/openai/streaming.h` declares Responses streaming types and functions
- [ ] `src/providers/openai/streaming_responses.c` implements async event processing
- [ ] Stream context holds stream_cb and stream_ctx ONLY (completion callback passed to start_stream() instead)
- [ ] `ik_openai_responses_stream_ctx_create()` takes stream_cb and stream_ctx parameters only (NOT completion callback)
- [ ] Completion callback handled by `start_stream()` vtable method and curl_multi infrastructure
- [ ] Write callback (`ik_openai_responses_stream_write_callback`) feeds data to SSE parser
- [ ] SSE parser invokes `process_event` for each complete event
- [ ] `process_event` invokes stream_cb with normalized events (during perform context)
- [ ] response.created emits IK_STREAM_START event
- [ ] response.output_text.delta emits IK_STREAM_TEXT_DELTA
- [ ] response.reasoning_summary_text.delta emits IK_STREAM_THINKING_DELTA
- [ ] response.output_item.added (function_call) emits IK_STREAM_TOOL_CALL_START
- [ ] response.function_call_arguments.delta emits IK_STREAM_TOOL_CALL_DELTA
- [ ] response.output_item.done emits IK_STREAM_TOOL_CALL_DONE
- [ ] response.completed emits IK_STREAM_DONE with status mapping and usage
- [ ] error event emits IK_STREAM_ERROR with category mapping
- [ ] Usage extracted from completed event including reasoning_tokens
- [ ] Finish reason mapping uses `ik_openai_map_responses_status()`
- [ ] Makefile updated with streaming_responses.c
- [ ] All tests pass (verify: `make check`)
- [ ] Compiles without warnings
- [ ] Changes committed to git with message: `task: openai-streaming-responses.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).