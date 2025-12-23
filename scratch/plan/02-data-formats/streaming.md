# Streaming Event Normalization

## Overview

All providers emit normalized streaming events through a unified `ik_stream_event_t` type. Provider adapters are responsible for transforming provider-specific SSE events into this normalized format.

**Key principle:** Streaming is inherently async. Events are delivered via callbacks as data arrives during `perform()` calls in the event loop.

## Stream Event Types

The system defines the following stream event type enum:

```c
typedef enum {
    IK_STREAM_START,           // Stream started
    IK_STREAM_TEXT_DELTA,      // Text content chunk
    IK_STREAM_THINKING_DELTA,  // Thinking/reasoning chunk
    IK_STREAM_TOOL_CALL_START, // Tool call started
    IK_STREAM_TOOL_CALL_DELTA, // Tool call argument chunk
    IK_STREAM_TOOL_CALL_DONE,  // Tool call complete
    IK_STREAM_DONE,            // Stream complete
    IK_STREAM_ERROR            // Error occurred
} ik_stream_event_type_t;
```

## Stream Event Structure

The `ik_stream_event_t` structure contains:

```c
typedef struct {
    ik_stream_event_type_t type;
    // ... other fields
} ik_stream_event_t;
```

- **type**: The event type (from the enum above)
- **data**: A union containing event-specific data based on type:

### Event Data Fields by Type

**start**:
- `model` - Model being used

**text_delta**:
- `text` - Text fragment
- `index` - Content block index

**thinking_delta**:
- `text` - Thinking fragment
- `index` - Thinking block index

**tool_call_start**:
- `id` - Tool call ID
- `name` - Function name
- `index` - Tool call index

**tool_call_delta**:
- `index` - Tool call index
- `arguments` - JSON fragment

**tool_call_done**:
- `index` - Tool call index

**done**:
- `finish_reason` - Reason stream completed
- `usage` - Token usage information
- `provider_data` - Opaque metadata from provider

**error**:
- `error` - Error details (ik_provider_error_t)

## Provider Event Mapping

### Anthropic SSE Events

**Provider events:**
- `message_start` - Stream started
- `content_block_start` - New content block (text or tool_use)
- `content_block_delta` - Content delta (text_delta or input_json_delta)
- `content_block_stop` - Content block completed
- `message_delta` - Metadata update (stop_reason, usage)
- `message_stop` - Stream completed
- `error` - Error occurred

**Mapping:**

| Anthropic Event | ikigai Event | Notes |
|-----------------|--------------|-------|
| `message_start` | `IK_STREAM_START` | Extract model from message.model |
| `content_block_delta` (text_delta) | `IK_STREAM_TEXT_DELTA` | Extract delta.text |
| `content_block_delta` (thinking_delta) | `IK_STREAM_THINKING_DELTA` | Extract delta.text (Anthropic 3.7+) |
| `content_block_start` (tool_use) | `IK_STREAM_TOOL_CALL_START` | Extract id, name |
| `content_block_delta` (input_json_delta) | `IK_STREAM_TOOL_CALL_DELTA` | Extract partial_json |
| `content_block_stop` (tool_use) | `IK_STREAM_TOOL_CALL_DONE` | Index from content_block_start |
| `message_delta` | Update internal state | Accumulate usage, stop_reason |
| `message_stop` | `IK_STREAM_DONE` | Emit final usage/finish_reason |
| `error` | `IK_STREAM_ERROR` | Map error to ik_provider_error_t |

### OpenAI SSE Events (Responses API)

**Provider events:**
- `response.created` - Response started
- `response.output_item.added` - New output item
- `response.output_text.delta` - Text delta
- `response.reasoning_summary_text.delta` - Reasoning summary delta
- `response.function_call_arguments.delta` - Tool call arguments delta
- `response.completed` - Stream completed
- `error` - Error occurred

**Mapping:**

| OpenAI Event | ikigai Event | Notes |
|--------------|--------------|-------|
| `response.created` | `IK_STREAM_START` | Extract model from response |
| `response.output_text.delta` | `IK_STREAM_TEXT_DELTA` | Extract delta.text |
| `response.reasoning_summary_text.delta` | `IK_STREAM_THINKING_DELTA` | Reasoning summary |
| `response.function_call_arguments.delta` | `IK_STREAM_TOOL_CALL_DELTA` | Extract delta |
| `response.output_item.added` (function_call) | `IK_STREAM_TOOL_CALL_START` | Extract name, id |
| `response.output_item.done` (function_call) | `IK_STREAM_TOOL_CALL_DONE` | Tool call completed |
| `response.completed` | `IK_STREAM_DONE` | Extract usage, status |
| `error` | `IK_STREAM_ERROR` | Map error |

### Google SSE Events

**Provider events:**
- `candidates[].content.parts[]` - Content parts
- `candidates[].finishReason` - Finish reason
- `usageMetadata` - Token usage

**Notes:**
- Google streams complete chunks, not fine-grained deltas
- Each chunk may contain multiple parts
- Thinking content has `thought: true` flag

**Mapping:**

| Google Event | ikigai Event | Notes |
|--------------|--------------|-------|
| First chunk | `IK_STREAM_START` | Extract model from request |
| Part (text, !thought) | `IK_STREAM_TEXT_DELTA` | Extract text |
| Part (text, thought=true) | `IK_STREAM_THINKING_DELTA` | Extract text |
| Part (functionCall) | `IK_STREAM_TOOL_CALL_START` + `IK_STREAM_TOOL_CALL_DONE` | Complete in one chunk |
| Last chunk (finishReason) | `IK_STREAM_DONE` | Extract usageMetadata |
| Error chunk | `IK_STREAM_ERROR` | Map error |

## Data Flow

### Async Streaming Architecture

Streaming integrates with the select()-based event loop:

```
1. REPL calls provider->vt->start_stream(ctx, req, stream_cb, stream_ctx, ...)
   └── Provider configures curl easy handle with write callback
   └── Adds easy handle to curl_multi
   └── Returns immediately (non-blocking)

2. REPL event loop runs:
   ┌──────────────────────────────────────────────────────────────────────┐
   │ while (!done) {                                                      │
   │     provider->vt->fdset(ctx, &read_fds, &write_fds, &exc_fds, &max);│
   │     select(max+1, &read_fds, &write_fds, &exc_fds, &timeout);       │
   │                                                                      │
   │     provider->vt->perform(ctx, &running);                            │
   │     // ↑ This triggers curl write callbacks as data arrives          │
   │     // ↑ Write callbacks invoke SSE parser                           │
   │     // ↑ SSE parser invokes stream_cb with normalized events         │
   │                                                                      │
   │     provider->vt->info_read(ctx, logger);                            │
   │     // ↑ When transfer completes, invokes completion_cb              │
   │ }                                                                    │
   └──────────────────────────────────────────────────────────────────────┘
```

### Provider Adapter Flow (Inside perform())

1. **curl write callback triggered**: Data arrives from network
2. **Feed to SSE parser**: `ik_sse_parse_chunk(parser, data, len)`
3. **SSE parser emits events**: For each complete "event:" + "data:" pair
4. **Provider handler invoked**: Receives raw event type and JSON data
5. **Parse JSON**: Using yyjson
6. **Map to normalized event**: Create `ik_stream_event_t` with appropriate type
7. **Invoke user callback**: `stream_cb(event, stream_ctx)`
8. **State tracking**: Maintain internal state for multi-event sequences

### REPL Consumption Flow

The REPL receives normalized events through its stream callback and processes them based on type:

- **IK_STREAM_START**: Initialize streaming response object
- **IK_STREAM_TEXT_DELTA**: Append text to scrollback buffer, trigger UI render
- **IK_STREAM_THINKING_DELTA**: Append to thinking area (if visible), trigger UI render
- **IK_STREAM_TOOL_CALL_START**: Initialize tool call accumulator with id and name
- **IK_STREAM_TOOL_CALL_DELTA**: Append JSON fragments to arguments buffer
- **IK_STREAM_TOOL_CALL_DONE**: Finalize accumulated JSON, parse, and execute tool
- **IK_STREAM_DONE**: Save complete response to database, update token counts in UI
- **IK_STREAM_ERROR**: Display error message in scrollback

**Important:** Stream callbacks are invoked from within `perform()`, which is called from the REPL event loop. The UI can be updated incrementally because control returns to the event loop between network activity.

## Error Handling

### Mid-Stream Errors

When a provider adapter detects an error during streaming (network failure, invalid data, etc.):
1. Create `IK_STREAM_ERROR` event with appropriate error details
2. Emit error event to user callback
3. Stop processing further events
4. REPL displays error and marks response as failed

### Incomplete Streams

If a stream ends without receiving `IK_STREAM_DONE`:
- REPL detects missing completion event
- Displays warning about incomplete response
- Saves partial response with "incomplete" status
- Allows user to retry or continue conversation

## Content Block Indexing

Multiple content blocks may stream in parallel (rare but possible with some providers). The index field in delta events allows tracking which block each fragment belongs to:

**Example sequence:**
1. Text block 0: "Hello" -> " world"
2. Tool call block 1: START(id="call_1", name="bash") -> DELTA(args) -> DONE
3. Text block 0: "!" (continues after tool call)

**REPL handling:**
- Maintains array of active content blocks
- Each block tracks its type (text, thinking, tool_call)
- Text/thinking blocks use string builders for efficient accumulation
- Tool call blocks accumulate id, name, and argument fragments
- Index field routes deltas to correct accumulator

## Performance Considerations

### Buffering Strategy

The REPL buffers text deltas before rendering to avoid excessive UI updates:
- Collects deltas for 16ms intervals
- Renders UI once per interval
- Balances responsiveness with CPU efficiency

### String Accumulation

Efficient string building uses `talloc_string_builder_t`:
- Avoids repeated reallocations
- Tracks capacity and length separately
- Amortized O(1) append operations
- Single finalization to immutable string

## Testing Approach

Mock streaming tests:
1. Create array of SSE event strings (event + data pairs)
2. Feed to provider adapter
3. Capture normalized events emitted to callback
4. Verify correct types, data extraction, and sequencing
5. Test error conditions (malformed JSON, missing fields, connection drops)
