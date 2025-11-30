# Bug: Tool Calls Not Parsed from SSE Stream

## Problem

When OpenAI responds with a tool call instead of text content, the tool call data is silently dropped. The LLM is correctly offered tools and chooses to use them, but we never see the response.

## Root Cause

The SSE stream parser only extracts `delta.content` (text) and ignores `delta.tool_calls`.

### Code Flow

1. **`http_write_callback`** (`src/openai/client_multi_callbacks.c:229`)
   - Calls `ik_openai_parse_sse_event()` to extract content
   - When content is NULL, does nothing with the event

2. **`ik_openai_parse_sse_event`** (`src/openai/sse_parser.c:138-204`)
   - Only looks for `choices[0].delta.content`
   - Returns `OK(NULL)` when content is missing (lines 189-193)
   - Tool call data in `delta.tool_calls` is never examined

3. **`ik_openai_parse_tool_calls`** (`src/openai/sse_parser.c:206+`)
   - Exists and correctly parses `choices[0].delta.tool_calls[0]`
   - **Never called from anywhere**

4. **`http_write_ctx_t`** (`src/openai/client_multi_callbacks.h:15-25`)
   - No field to store tool call data
   - Only has `complete_response` for text accumulation

## Fix Required

### 1. Add tool_call field to context struct

`src/openai/client_multi_callbacks.h`:
```c
typedef struct {
    ik_openai_sse_parser_t *parser;
    ik_openai_stream_cb_t user_callback;
    void *user_ctx;
    char *complete_response;
    size_t response_len;
    bool has_error;
    char *model;
    char *finish_reason;
    int32_t completion_tokens;
    ik_tool_call_t *tool_call;           /* NEW: Tool call if present */
} http_write_ctx_t;
```

### 2. Parse tool_calls when content is NULL

`src/openai/client_multi_callbacks.c` in `http_write_callback`, after line 235:
```c
char *content = content_res.ok;
if (content != NULL) {
    // ... existing content handling ...
} else {
    // Check for tool calls when no content
    res_t tool_res = ik_openai_parse_tool_calls(ctx->parser, event);
    if (is_ok(&tool_res) && tool_res.ok != NULL) {
        ik_tool_call_t *tc = tool_res.ok;
        if (ctx->tool_call == NULL) {
            // First tool call chunk - store it
            ctx->tool_call = talloc_steal(ctx->parser, tc);
        } else {
            // Accumulate arguments (streaming case)
            // Append tc->arguments to ctx->tool_call->arguments
        }
    }
}
```

### 3. Propagate tool_call to caller

The caller of the HTTP request needs to check for `ctx->tool_call` after the request completes and handle tool execution.

## Files to Modify

1. `src/openai/client_multi_callbacks.h` - Add tool_call field
2. `src/openai/client_multi_callbacks.c` - Call parse_tool_calls, accumulate result
3. `src/openai/client_multi.c` (likely) - Propagate tool_call to response struct
4. Caller code - Handle tool execution when tool_call is present
