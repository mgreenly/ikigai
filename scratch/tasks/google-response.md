# Task: Google Response Parsing

**Model:** sonnet/thinking
**Depends on:** google-core.md, google-request.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/providers/provider.h` - Response types
- `src/providers/google/request.h` - Request serialization

**Plan:**
- `scratch/plan/transformation.md` - Response transformation rules
- `scratch/plan/error-handling.md` - Error mapping

## Objective

Implement response parsing for Google Gemini API. Transform Google JSON response to internal `ik_response_t` format, parse error responses, and implement the non-streaming `send` vtable function.

## Key Differences from Anthropic

| Field | Anthropic | Google |
|-------|-----------|--------|
| Content location | `content[]` | `candidates[0].content.parts[]` |
| Thinking marker | `type: "thinking"` | `thought: true` flag |
| Tool call | `type: "tool_use"` | `functionCall` object |
| Tool call ID | Provider-generated | **We must generate** |
| Finish reason | `stop_reason` string | `finishReason` enum |
| Token usage | `usage` | `usageMetadata` |

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_google_parse_response(TALLOC_CTX *ctx, const char *json, size_t json_len, ik_response_t **out_resp)` | Parse Google JSON response to internal format, returns OK/ERR |
| `res_t ik_google_parse_error(TALLOC_CTX *ctx, int http_status, const char *json, size_t json_len, ik_error_category_t *out_category, char **out_message)` | Parse error response and map to category, returns OK/ERR |
| `ik_finish_reason_t ik_google_map_finish_reason(const char *finish_reason)` | Map Google finishReason string to internal enum |
| `char *ik_google_generate_tool_id(TALLOC_CTX *ctx)` | Generate 22-character base64url tool call ID |
| `res_t ik_google_start_request(void *impl_ctx, const ik_request_t *req, ik_provider_completion_cb_t cb, void *cb_ctx)` | Async vtable start_request implementation (non-streaming) |
| `res_t ik_google_start_stream(void *impl_ctx, const ik_request_t *req, ik_stream_cb_t stream_cb, void *stream_ctx, ik_provider_completion_cb_t completion_cb, void *completion_ctx)` | Async vtable start_stream implementation (streaming) |

## Behaviors

**Response Parsing:**
- Parse JSON using yyjson
- Check for error object first, return ERR if present
- Check for promptFeedback.blockReason (blocked prompt), return ERR if present
- Extract modelVersion field into response.model
- Parse usageMetadata: promptTokenCount, candidatesTokenCount, thoughtsTokenCount, totalTokenCount
- Calculate output_tokens = candidatesTokenCount - thoughtsTokenCount
- Extract first candidate from candidates array
- Parse finishReason and map to internal enum
- Extract content.parts[] array
- For each part, determine type and create content block:
  - Part with functionCall: IK_CONTENT_TOOL_CALL with generated ID
  - Part with thought=true flag: IK_CONTENT_THINKING
  - Part with text only: IK_CONTENT_TEXT
- Return empty response (content_count=0) if no candidates or no parts
- All allocations use talloc

**Tool ID Generation:**
- Generate 22-character random base64url string
- Use alphabet: A-Z, a-z, 0-9, -, _
- Seed random generator on first use with time()
- Return talloc-allocated string

**Finish Reason Mapping:**
- "STOP" -> IK_FINISH_STOP
- "MAX_TOKENS" -> IK_FINISH_LENGTH
- "SAFETY", "BLOCKLIST", "PROHIBITED_CONTENT", "IMAGE_SAFETY", "IMAGE_PROHIBITED_CONTENT", "RECITATION" -> IK_FINISH_CONTENT_FILTER
- "MALFORMED_FUNCTION_CALL", "UNEXPECTED_TOOL_CALL" -> IK_FINISH_ERROR
- NULL or unknown -> IK_FINISH_UNKNOWN

**Error Parsing:**
- Map HTTP status to error category:
  - 400 -> IK_ERR_CAT_INVALID_ARG
  - 401, 403 -> IK_ERR_CAT_AUTH
  - 404 -> IK_ERR_CAT_NOT_FOUND
  - 429 -> IK_ERR_CAT_RATE_LIMIT
  - 500, 502, 503 -> IK_ERR_CAT_SERVER
  - 504 -> IK_ERR_CAT_TIMEOUT
  - Other -> IK_ERR_CAT_UNKNOWN
- Extract error.message from JSON if present
- Format message as "status: message" or "HTTP {code}" if no JSON

**start_request() Implementation (Async Pattern):**
- Cast `impl_ctx` to Google provider context
- Serialize request using ik_google_serialize_request()
- Build URL using ik_google_build_url() with streaming=false
- Build headers using ik_google_build_headers() with streaming=false
- Build `ik_http_request_t` with method="POST", url, headers, body
- Call `ik_http_multi_add_request()` with write callback and completion callback
- **Return immediately** (non-blocking) - do NOT wait for response
- Write callback accumulates response body into buffer
- Completion callback:
  - Check HTTP status, parse error if >= 400 using ik_google_parse_error()
  - Parse response body using ik_google_parse_response()
  - Create `ik_provider_completion_t` with result
  - Invoke user's `ik_provider_completion_cb_t` callback with completion

**start_stream() Implementation (Async Pattern):**
- Cast `impl_ctx` to Google provider context
- Serialize request using ik_google_serialize_request()
- Build URL using ik_google_build_url() with streaming=true
- Build headers using ik_google_build_headers() with streaming=true
- Build `ik_http_request_t` with method="POST", url, headers, body
- Call `ik_http_multi_add_request()` with stream write callback and completion callback
- **Return immediately** (non-blocking) - do NOT wait for response
- Stream write callback:
  - Parse SSE chunks as they arrive
  - Invoke `ik_stream_cb_t` callback with stream events (TEXT_DELTA, TOOL_CALL_START, etc.)
- Completion callback:
  - Send final DONE or ERROR event
  - Invoke user's `ik_provider_completion_cb_t` callback with completion

## Test Scenarios

**Simple Text Response:**
- Parse response with single text part in candidates[0].content.parts[]
- Verify model, finish_reason, content_count, content type
- Verify usage tokens extracted correctly

**Thinking Response:**
- Parse response with thought=true part followed by text part
- First content block is IK_CONTENT_THINKING
- Second content block is IK_CONTENT_TEXT
- Verify thinking_tokens and output_tokens calculated correctly (output = candidates - thoughts)

**Function Call Response:**
- Parse response with functionCall part
- Content block type is IK_CONTENT_TOOL_CALL
- Tool call ID is generated (22 characters)
- Tool name and arguments extracted

**Finish Reason Mapping:**
- "STOP" maps to IK_FINISH_STOP
- "MAX_TOKENS" maps to IK_FINISH_LENGTH
- "SAFETY" maps to IK_FINISH_CONTENT_FILTER
- "MALFORMED_FUNCTION_CALL" maps to IK_FINISH_ERROR
- NULL or "UNKNOWN" maps to IK_FINISH_UNKNOWN

**Error Response:**
- Response with error object returns ERR
- Error message extracted from JSON

**Blocked Prompt:**
- Response with promptFeedback.blockReason returns ERR
- Error message includes block reason

**Error Status Codes:**
- 401 status maps to IK_ERR_CAT_AUTH
- 429 status maps to IK_ERR_CAT_RATE_LIMIT
- 500 status maps to IK_ERR_CAT_SERVER
- 504 status maps to IK_ERR_CAT_TIMEOUT

**Tool ID Generation:**
- Generated IDs are 22 characters long
- IDs contain only base64url characters (A-Z, a-z, 0-9, -, _)
- Multiple calls produce different IDs

**Empty Response:**
- Response with no candidates returns empty content array
- No error returned for empty response

## Postconditions

- [ ] `src/providers/google/response.h` exists
- [ ] `src/providers/google/response.c` implements parsing
- [ ] `ik_google_parse_response()` extracts all part types
- [ ] Text parts parsed as `IK_CONTENT_TEXT`
- [ ] Parts with `thought: true` parsed as `IK_CONTENT_THINKING`
- [ ] `functionCall` parsed as `IK_CONTENT_TOOL_CALL` with generated ID
- [ ] `ik_google_generate_tool_id()` creates 22-char base64url IDs
- [ ] Usage correctly extracts prompt/candidates/thoughts tokens
- [ ] Finish reason correctly mapped for all finishReason values
- [ ] `ik_google_parse_error()` maps HTTP status to category
- [ ] Blocked prompts (`promptFeedback.blockReason`) return error
- [ ] `ik_google_start_request()` and `ik_google_start_stream()` wired to vtable in google.c
- [ ] Makefile updated with response.c
- [ ] All response parsing tests pass
- [ ] Changes committed to git with message: `task: google-response.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).