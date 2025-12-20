# Streaming Event Normalization

## Overview

All providers emit normalized streaming events through a unified `ik_stream_event_t` type. Provider adapters are responsible for transforming provider-specific SSE events into this normalized format.

## Stream Event Types

```c
typedef enum {
    IK_STREAM_START,           // Stream started
    IK_STREAM_TEXT_DELTA,      // Text content delta
    IK_STREAM_THINKING_DELTA,  // Thinking/reasoning delta
    IK_STREAM_TOOL_CALL_START, // Tool call started
    IK_STREAM_TOOL_CALL_DELTA, // Tool call arguments delta
    IK_STREAM_TOOL_CALL_DONE,  // Tool call completed
    IK_STREAM_DONE,            // Stream completed successfully
    IK_STREAM_ERROR            // Error occurred
} ik_stream_event_type_t;
```

## Stream Event Structure

```c
typedef struct ik_stream_event {
    ik_stream_event_type_t type;

    union {
        struct {
            char *model;           // Model being used
        } start;

        struct {
            char *text;            // Text fragment
            int index;             // Content block index
        } text_delta;

        struct {
            char *text;            // Thinking fragment
            int index;             // Thinking block index
        } thinking_delta;

        struct {
            char *id;              // Tool call ID
            char *name;            // Function name
            int index;             // Tool call index
        } tool_call_start;

        struct {
            int index;             // Tool call index
            char *arguments;       // JSON fragment
        } tool_call_delta;

        struct {
            int index;             // Tool call index
        } tool_call_done;

        struct {
            ik_finish_reason_t finish_reason;
            ik_usage_t usage;
            yyjson_val *provider_data;  // Opaque metadata
        } done;

        struct {
            ik_error_t error;
        } error;

    } data;

} ik_stream_event_t;
```

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
| `error` | `IK_STREAM_ERROR` | Map error to ik_error_t |

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

## Adapter Implementation Pattern

### Anthropic Streaming Adapter

```c
static void anthropic_sse_callback(const char *event,
                                   const char *data,
                                   void *user_ctx)
{
    ik_anthropic_stream_ctx_t *ctx = user_ctx;

    if (strcmp(event, "message_start") == 0) {
        // Parse JSON
        yyjson_doc *doc = yyjson_read(data, strlen(data), 0);
        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *message = yyjson_obj_get(root, "message");
        const char *model = yyjson_get_str(yyjson_obj_get(message, "model"));

        // Emit normalized event
        ik_stream_event_t norm_event = {
            .type = IK_STREAM_START,
            .data.start.model = model
        };
        ctx->user_cb(&norm_event, ctx->user_ctx);

        yyjson_doc_free(doc);
    }
    else if (strcmp(event, "content_block_delta") == 0) {
        yyjson_doc *doc = yyjson_read(data, strlen(data), 0);
        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *delta = yyjson_obj_get(root, "delta");
        const char *type = yyjson_get_str(yyjson_obj_get(delta, "type"));

        if (strcmp(type, "text_delta") == 0) {
            const char *text = yyjson_get_str(yyjson_obj_get(delta, "text"));
            int index = yyjson_get_int(yyjson_obj_get(root, "index"));

            ik_stream_event_t norm_event = {
                .type = IK_STREAM_TEXT_DELTA,
                .data.text_delta = {
                    .text = text,
                    .index = index
                }
            };
            ctx->user_cb(&norm_event, ctx->user_ctx);
        }
        else if (strcmp(type, "thinking_delta") == 0) {
            const char *text = yyjson_get_str(yyjson_obj_get(delta, "text"));
            int index = yyjson_get_int(yyjson_obj_get(root, "index"));

            ik_stream_event_t norm_event = {
                .type = IK_STREAM_THINKING_DELTA,
                .data.thinking_delta = {
                    .text = text,
                    .index = index
                }
            };
            ctx->user_cb(&norm_event, ctx->user_ctx);
        }

        yyjson_doc_free(doc);
    }
    else if (strcmp(event, "message_stop") == 0) {
        // Emit final event
        ik_stream_event_t norm_event = {
            .type = IK_STREAM_DONE,
            .data.done = {
                .finish_reason = ctx->finish_reason,
                .usage = ctx->usage,
                .provider_data = NULL
            }
        };
        ctx->user_cb(&norm_event, ctx->user_ctx);
    }
}
```

## REPL Stream Callback

REPL receives normalized events:

```c
// src/repl_streaming.c

void ik_repl_stream_callback(ik_stream_event_t *event, void *user_ctx)
{
    ik_repl_ctx_t *repl = user_ctx;

    switch (event->type) {
        case IK_STREAM_START:
            // Initialize response
            repl->streaming_response = create_response();
            break;

        case IK_STREAM_TEXT_DELTA:
            // Append to scrollback
            ik_scrollback_append_text(repl->scrollback,
                                     event->data.text_delta.text);
            ik_ui_render(repl->ui);  // Update UI
            break;

        case IK_STREAM_THINKING_DELTA:
            // Append to thinking area (if visible)
            ik_scrollback_append_thinking(repl->scrollback,
                                         event->data.thinking_delta.text);
            ik_ui_render(repl->ui);
            break;

        case IK_STREAM_TOOL_CALL_START:
            // Start accumulating tool call
            start_tool_call(repl, event->data.tool_call_start.id,
                           event->data.tool_call_start.name);
            break;

        case IK_STREAM_TOOL_CALL_DELTA:
            // Accumulate arguments JSON
            append_tool_call_args(repl, event->data.tool_call_delta.index,
                                 event->data.tool_call_delta.arguments);
            break;

        case IK_STREAM_TOOL_CALL_DONE:
            // Finalize tool call, execute it
            finalize_and_execute_tool_call(repl, event->data.tool_call_done.index);
            break;

        case IK_STREAM_DONE:
            // Save to database
            save_assistant_message(repl->agent,
                                  repl->streaming_response,
                                  event->data.done.usage);

            // Update token display
            update_token_counts(repl->ui, event->data.done.usage);
            break;

        case IK_STREAM_ERROR:
            // Display error
            display_error(repl->scrollback, event->data.error.error.message);
            break;
    }
}
```

## Error Handling During Streaming

### Mid-Stream Errors

```c
// Provider detects error during streaming
if (http_error) {
    ik_stream_event_t error_event = {
        .type = IK_STREAM_ERROR,
        .data.error.error = {
            .category = ERR_NETWORK,
            .message = "Connection lost during streaming"
        }
    };
    user_cb(&error_event, user_ctx);
    return;  // Stop streaming
}
```

### Incomplete Streams

If stream ends without `IK_STREAM_DONE`:

```c
// REPL detects incomplete stream
if (!received_done_event) {
    // Treat as error
    display_warning("Stream ended unexpectedly. Response may be incomplete.");

    // Save partial response with warning
    save_partial_response(agent, partial_text, "incomplete");
}
```

## Content Block Indexing

Multiple content blocks may stream in parallel (rare but possible):

```c
// Text block 0
{.type = IK_STREAM_TEXT_DELTA, .data.text_delta.index = 0, .text = "Hello"}
{.type = IK_STREAM_TEXT_DELTA, .data.text_delta.index = 0, .text = " world"}

// Tool call block 1
{.type = IK_STREAM_TOOL_CALL_START, .data.tool_call_start.index = 1, .id = "call_1", .name = "bash"}
{.type = IK_STREAM_TOOL_CALL_DELTA, .data.tool_call_delta.index = 1, .arguments = "{\"com"}
{.type = IK_STREAM_TOOL_CALL_DELTA, .data.tool_call_delta.index = 1, .arguments = "mand\":\"ls\"}"}
{.type = IK_STREAM_TOOL_CALL_DONE, .data.tool_call_done.index = 1}

// Text block 0 continues
{.type = IK_STREAM_TEXT_DELTA, .data.text_delta.index = 0, .text = "!"}
```

REPL maintains array of active blocks:

```c
typedef struct {
    ik_content_type_t type;
    talloc_string_builder_t *text_builder;  // For text/thinking
    char *tool_call_id;                     // For tool calls
    char *tool_name;
    talloc_string_builder_t *args_builder;
} ik_active_content_block_t;

ik_active_content_block_t blocks[10];  // Max 10 concurrent blocks
```

## Performance Considerations

### Buffering

REPL may buffer deltas before rendering:

```c
// Buffer text deltas for 16ms, then render once
#define RENDER_INTERVAL_MS 16

if (time_since_last_render() >= RENDER_INTERVAL_MS) {
    ik_ui_render(repl->ui);
    last_render_time = now();
}
```

### String Building

Use `talloc_string_builder_t` for efficient string accumulation:

```c
talloc_string_builder_t *builder = talloc_string_builder_create(ctx);

// Append deltas
for each delta:
    talloc_string_builder_append(builder, delta_text);

// Finalize
char *complete_text = talloc_string_builder_finalize(builder);
```

## Testing

### Mock Streaming

```c
START_TEST(test_anthropic_streaming) {
    // Mock SSE stream
    const char *sse_events[] = {
        "event: message_start\ndata: {\"message\":{\"model\":\"claude-sonnet-4-5\"}}\n\n",
        "event: content_block_delta\ndata: {\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}\n\n",
        "event: message_stop\ndata: {}\n\n",
        NULL
    };

    mock_sse_stream(sse_events);

    // Verify normalized events received
    ik_stream_event_t events[3];
    int event_count = 0;

    auto callback = lambda(void, (ik_stream_event_t *e, void *ctx) {
        events[event_count++] = *e;
    });

    ik_anthropic_stream(provider, req, callback, NULL);

    ck_assert_int_eq(event_count, 3);
    ck_assert_int_eq(events[0].type, IK_STREAM_START);
    ck_assert_int_eq(events[1].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events[1].data.text_delta.text, "Hello");
    ck_assert_int_eq(events[2].type, IK_STREAM_DONE);
}
END_TEST
```
