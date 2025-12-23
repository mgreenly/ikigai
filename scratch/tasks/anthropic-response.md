# Task: Anthropic Response Parsing

**Model:** sonnet/thinking
**Depends on:** anthropic-core.md, anthropic-request.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - Talloc-based memory management

**Source:**
- `src/providers/provider.h` - Response types
- `src/providers/anthropic/request.h` - Request serialization
- `src/json_allocator.h` - Talloc-integrated JSON allocators

**Plan:**
- `scratch/plan/configuration.md` - Response transformation rules and error mapping

## Objective

Implement response parsing for Anthropic API. Transform Anthropic JSON response to internal `ik_response_t` format, including content blocks, usage statistics, and finish reasons. Also implement the async `start_request()` and `start_stream()` vtable functions that follow the non-blocking pattern required by the select()-based event loop.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_anthropic_parse_response(TALLOC_CTX *ctx, const char *json, size_t json_len, ik_response_t **out_resp)` | Parses Anthropic JSON response to internal format, returns OK with response or ERR on parse error |
| `res_t ik_anthropic_parse_error(TALLOC_CTX *ctx, int http_status, const char *json, size_t json_len, ik_error_category_t *out_category, char **out_message)` | Parses Anthropic error response, maps HTTP status to category, extracts message |
| `ik_finish_reason_t ik_anthropic_map_finish_reason(const char *stop_reason)` | Maps Anthropic stop_reason string to internal finish reason enum |
| `res_t ik_anthropic_start_request(void *impl_ctx, const ik_request_t *req, ik_provider_completion_cb_t cb, void *cb_ctx)` | Async vtable start_request implementation: serialize request, start non-blocking HTTP POST, returns immediately |
| `res_t ik_anthropic_start_stream(void *impl_ctx, const ik_request_t *req, ik_stream_cb_t stream_cb, void *stream_ctx, ik_provider_completion_cb_t completion_cb, void *completion_ctx)` | Async vtable start_stream implementation: serialize streaming request, start non-blocking HTTP POST, returns immediately |

Structs to define: None (uses existing provider types)

## Behaviors

### Response Parsing
- When `ik_anthropic_parse_response()` is called, parse JSON with yyjson
- Extract model name, copy to response
- Extract stop_reason, map to finish_reason
- Extract usage statistics (input_tokens, output_tokens, thinking_tokens, cache_read_input_tokens)
- Calculate total_tokens as sum of input, output, and thinking tokens
- Parse content array into content blocks
- Return ERR(PARSE) if JSON is invalid or root is not object
- Check for error response (type="error"), return ERR(PROVIDER) with message

### Content Block Parsing
- For each content block in array:
  - type="text" → IK_CONTENT_TEXT with text field
  - type="thinking" → IK_CONTENT_THINKING with thinking field
  - type="tool_use" → IK_CONTENT_TOOL_CALL with id, name, input (as JSON value)
  - type="redacted_thinking" → IK_CONTENT_THINKING with text "[thinking redacted]"
  - Unknown type → log warning, continue parsing
- Store borrowed JSON reference for tool_call.arguments (immutable)

### Usage Parsing
- Extract input_tokens, output_tokens from usage object
- Extract thinking_tokens if present (optional field for some models)
- Extract cache_read_input_tokens if present, store as cached_tokens
- Calculate total_tokens
- If usage object is NULL, set all fields to 0

### Finish Reason Mapping
- "end_turn" → IK_FINISH_STOP
- "max_tokens" → IK_FINISH_LENGTH
- "tool_use" → IK_FINISH_TOOL_USE
- "stop_sequence" → IK_FINISH_STOP
- "refusal" → IK_FINISH_CONTENT_FILTER
- NULL or unknown → IK_FINISH_UNKNOWN

### Error Parsing
- Map HTTP status to error category:
  - 400 → IK_ERR_CAT_INVALID_ARG
  - 401, 403 → IK_ERR_CAT_AUTH
  - 404 → IK_ERR_CAT_NOT_FOUND
  - 429 → IK_ERR_CAT_RATE_LIMIT
  - 500, 502, 503, 529 → IK_ERR_CAT_SERVER
  - Other → IK_ERR_CAT_UNKNOWN
- Extract error.message and error.type from JSON if present
- Format message as "type: message" or "HTTP status" if JSON unavailable

### Async start_request() Implementation
- Cast `impl_ctx` to Anthropic provider context
- Serialize request to JSON using `ik_anthropic_serialize_request()`
- Build headers using `ik_anthropic_build_headers()`
- Construct URL: base_url + "/v1/messages"
- Build `ik_http_request_t` with method="POST", url, headers, body
- Call `ik_http_multi_add_request()` with write callback and completion callback
- **Return immediately** (non-blocking) - do NOT wait for response
- Write callback accumulates response body into buffer
- Completion callback (invoked later from info_read()):
  - Check HTTP status, if >= 400 parse error using `ik_anthropic_parse_error()`
  - Parse successful response using `ik_anthropic_parse_response()`
  - Build `ik_provider_completion_t` with success flag, response, or error
  - Invoke user's `ik_provider_completion_cb_t` with completion struct

### Async start_stream() Implementation
- Cast `impl_ctx` to Anthropic provider context
- Serialize request to JSON using `ik_anthropic_serialize_request()` with streaming enabled
- Build headers using `ik_anthropic_build_headers()` (include "Accept: text/event-stream")
- Construct URL: base_url + "/v1/messages"
- Build `ik_http_request_t` with method="POST", url, headers, body
- Call `ik_http_multi_add_request()` with SSE parsing callbacks
- **Return immediately** (non-blocking) - do NOT wait for response
- SSE event callback parses stream events and invokes `ik_stream_cb_t`
- Completion callback (invoked later from info_read()):
  - Build `ik_provider_completion_t` with final state
  - Invoke user's `ik_provider_completion_cb_t` with completion struct

## Test Scenarios

### Simple Response Parsing (Unit)
- Response with single text block parses correctly
- Model name extracted
- Finish reason "end_turn" maps to IK_FINISH_STOP
- Usage tokens extracted correctly
- Content block has correct text

### Tool Use Response (Unit)
- Response with tool_use block parses correctly
- Tool call has id, name, and input JSON
- Finish reason "tool_use" maps to IK_FINISH_TOOL_USE

### Thinking Response (Unit)
- Response with thinking block parses correctly
- Thinking content extracted
- thinking_tokens counted in usage
- Multiple content blocks (thinking + text) parsed

### Redacted Thinking (Unit)
- Response with redacted_thinking block creates THINKING content
- Text is "[thinking redacted]"

### Finish Reason Mapping (Unit)
- All stop_reason strings map correctly
- NULL returns IK_FINISH_UNKNOWN
- Unknown string returns IK_FINISH_UNKNOWN

### Error Parsing (Unit)
- HTTP 401 maps to IK_ERR_CAT_AUTH
- HTTP 429 maps to IK_ERR_CAT_RATE_LIMIT
- HTTP 500 maps to IK_ERR_CAT_SERVER
- Error JSON message extracted and formatted
- Missing JSON returns default "HTTP status" message

### Error Response Detection (Unit)
- Response with type="error" returns ERR(PROVIDER)
- Error message extracted from error.message field

### Async start_request() Tests (With Mocked curl_multi)
- **start_request returns immediately**: Verify function returns OK before HTTP completes
- **Callback invoked from info_read**: Start request, simulate perform/info_read cycle, verify callback fires
- **Text response transform**: Verify response parsed correctly in callback
- **Tool call response**: Verify tool calls parsed and delivered via callback
- **Error response**: Verify error completion delivered to user callback
- **Missing API key**: start_request returns error immediately (before HTTP)

### Async start_stream() Tests (With Mocked curl_multi)
- **start_stream returns immediately**: Verify function returns OK before HTTP completes
- **Stream events delivered**: Simulate SSE events, verify stream_cb invoked with parsed events
- **Completion callback invoked**: Verify completion_cb fires at end of stream
- **Stream error handling**: Verify error delivered via completion_cb

## Postconditions

- [ ] `src/providers/anthropic/response.h` exists
- [ ] `src/providers/anthropic/response.c` implements parsing
- [ ] `ik_anthropic_parse_response()` extracts all content block types
- [ ] Text, thinking, tool_use, and redacted_thinking blocks parsed correctly
- [ ] Usage correctly extracts input/output/thinking/cached tokens
- [ ] Finish reason correctly mapped for all stop_reasons
- [ ] `ik_anthropic_parse_error()` maps HTTP status to category
- [ ] Error messages extracted from JSON when available
- [ ] `ik_anthropic_start_request()` and `ik_anthropic_start_stream()` wired to vtable in anthropic.c
- [ ] Functions use async pattern: return immediately, callbacks invoked from info_read()
- [ ] Functions accept const ik_request_t* (not mutable pointer)
- [ ] Callbacks receive ik_provider_completion_t struct with success/error info
- [ ] Compiles without warnings
- [ ] All response parsing tests pass
- [ ] All error mapping tests pass
- [ ] Changes committed to git with message: `task: anthropic-response.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).