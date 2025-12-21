# Task: Anthropic Response Parsing

**Model:** sonnet/thinking
**Depends on:** anthropic-core.md, anthropic-request.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

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

Implement response parsing for Anthropic API. Transform Anthropic JSON response to internal `ik_response_t` format, including content blocks, usage statistics, and finish reasons. Also implement the non-streaming `send` vtable function.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_anthropic_parse_response(TALLOC_CTX *ctx, const char *json, size_t json_len, ik_response_t **out_resp)` | Parses Anthropic JSON response to internal format, returns OK with response or ERR on parse error |
| `res_t ik_anthropic_parse_error(TALLOC_CTX *ctx, int http_status, const char *json, size_t json_len, ik_error_category_t *out_category, char **out_message)` | Parses Anthropic error response, maps HTTP status to category, extracts message |
| `ik_finish_reason_t ik_anthropic_map_finish_reason(const char *stop_reason)` | Maps Anthropic stop_reason string to internal finish reason enum |
| `res_t ik_anthropic_send_impl(void *impl_ctx, ik_request_t *req, ik_response_t **out_resp)` | Vtable send implementation: serialize request, POST to API, parse response |

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

### Send Implementation
- Serialize request to JSON using `ik_anthropic_serialize_request()`
- Build headers using `ik_anthropic_build_headers()`
- Construct URL: base_url + "/v1/messages"
- POST request using `ik_http_post()`
- Check HTTP status, if >= 400 parse error and return ERR(PROVIDER)
- Parse successful response using `ik_anthropic_parse_response()`
- Return OK with response

## Test Scenarios

### Simple Response Parsing
- Response with single text block parses correctly
- Model name extracted
- Finish reason "end_turn" maps to IK_FINISH_STOP
- Usage tokens extracted correctly
- Content block has correct text

### Tool Use Response
- Response with tool_use block parses correctly
- Tool call has id, name, and input JSON
- Finish reason "tool_use" maps to IK_FINISH_TOOL_USE

### Thinking Response
- Response with thinking block parses correctly
- Thinking content extracted
- thinking_tokens counted in usage
- Multiple content blocks (thinking + text) parsed

### Redacted Thinking
- Response with redacted_thinking block creates THINKING content
- Text is "[thinking redacted]"

### Finish Reason Mapping
- All stop_reason strings map correctly
- NULL returns IK_FINISH_UNKNOWN
- Unknown string returns IK_FINISH_UNKNOWN

### Error Parsing
- HTTP 401 maps to IK_ERR_CAT_AUTH
- HTTP 429 maps to IK_ERR_CAT_RATE_LIMIT
- HTTP 500 maps to IK_ERR_CAT_SERVER
- Error JSON message extracted and formatted
- Missing JSON returns default "HTTP status" message

### Error Response Detection
- Response with type="error" returns ERR(PROVIDER)
- Error message extracted from error.message field

## Postconditions

- [ ] `src/providers/anthropic/response.h` exists
- [ ] `src/providers/anthropic/response.c` implements parsing
- [ ] `ik_anthropic_parse_response()` extracts all content block types
- [ ] Text, thinking, tool_use, and redacted_thinking blocks parsed correctly
- [ ] Usage correctly extracts input/output/thinking/cached tokens
- [ ] Finish reason correctly mapped for all stop_reasons
- [ ] `ik_anthropic_parse_error()` maps HTTP status to category
- [ ] Error messages extracted from JSON when available
- [ ] `ik_anthropic_send_impl()` wired to vtable in anthropic.c
- [ ] Compiles without warnings
- [ ] All response parsing tests pass
- [ ] All error mapping tests pass
