# Task: OpenAI Send Implementation

**Layer:** 4
**Model:** sonnet/thinking
**Depends on:** openai-request-chat.md, openai-request-responses.md, openai-response-chat.md, openai-response-responses.md, http-client.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/openai/openai.c` - Provider context and vtable
- `src/providers/openai/request.h` - Request serialization
- `src/providers/openai/response.h` - Response parsing
- `src/providers/openai/reasoning.h` - Model detection
- `src/providers/common/http_client.h` - HTTP client

## Objective

Implement the `ik_openai_send_impl()` vtable function. This function decides which API to use (Chat Completions or Responses API), serializes the request, makes the HTTP call, and parses the response.

## API Selection Logic

```
if (context.use_responses_api) → use Responses API
else if (ik_openai_prefer_responses_api(model)) → use Responses API
else → use Chat Completions API
```

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_send_impl(impl_ctx, req, out_resp)` | Vtable send implementation, returns OK/ERR |

## Behaviors

### API Selection

- Check `ctx->use_responses_api` flag first (explicit override)
- If not set, call `ik_openai_prefer_responses_api(req->model)`
- Reasoning models (o1, o3) prefer Responses API
- Regular models (gpt-4o, gpt-5) use Chat Completions

### Request Flow

1. Create temporary talloc context for request-scoped allocations
2. Determine which API to use based on selection logic
3. Serialize request using appropriate serializer (chat or responses)
4. Build URL for chosen endpoint
5. Build HTTP headers with API key
6. Make HTTP POST request via `ik_http_post()`
7. Check HTTP status code for errors
8. Parse response using appropriate parser
9. Free temporary context
10. Return response

### Request Serialization

- Use `ik_openai_serialize_chat_request()` for Chat Completions
- Use `ik_openai_serialize_responses_request()` for Responses API
- Pass streaming=false for non-streaming requests
- Propagate serialization errors

### URL Building

- Use `ik_openai_build_chat_url()` for Chat Completions
- Use `ik_openai_build_responses_url()` for Responses API
- Pass `ctx->base_url` from provider context

### HTTP Request

- Use `ik_http_post()` with URL, headers, JSON body
- Capture response body, length, and HTTP status
- Propagate HTTP errors

### Error Handling

- HTTP status >= 400 indicates error
- Call `ik_openai_parse_error()` to extract error details
- Return ERR(PROVIDER) with error message
- Propagate all other errors (serialization, HTTP, parsing)

### Response Parsing

- Use `ik_openai_parse_chat_response()` for Chat Completions
- Use `ik_openai_parse_responses_response()` for Responses API
- Parse response body with length
- Allocate response on provider context (not temp context)
- Propagate parsing errors

### Memory Management

- Create temp context as child of provider context
- Allocate response on provider context (persists after temp freed)
- Free temp context before returning (success or error)
- Use talloc parent tracking for cleanup

## Test Scenarios

### API Selection - Reasoning Model

- Request with o1 model
- Verify `ik_openai_prefer_responses_api()` returns true
- Would use Responses API

### API Selection - Chat Model

- Request with gpt-4o model
- Verify `ik_openai_prefer_responses_api()` returns false
- Would use Chat Completions API

### API Selection - Override Flag

- Provider created with use_responses_api=true
- Request with gpt-4o model
- Should use Responses API despite model preference

### Send Success - Chat Completions

- Mock HTTP client for Chat Completions endpoint
- Verify correct serializer called
- Verify correct parser called
- Verify response returned

### Send Success - Responses API

- Mock HTTP client for Responses endpoint
- Verify correct serializer called
- Verify correct parser called
- Verify response returned

### HTTP Error Handling

- Mock HTTP returns 401 status
- Verify `ik_openai_parse_error()` called
- Verify ERR(PROVIDER) returned with message

### Serialization Error

- Request with invalid data
- Verify error propagated from serializer

### Parse Error

- HTTP returns malformed JSON
- Verify error propagated from parser

### Memory Cleanup

- Verify temp context freed on success
- Verify temp context freed on error
- Verify response allocated on provider context

## Postconditions

- [ ] `ik_openai_send_impl()` implemented in openai.c
- [ ] API selection logic correct for all model types
- [ ] use_responses_api flag properly respected
- [ ] Request serialization uses correct serializer
- [ ] Response parsing uses correct parser
- [ ] HTTP errors converted to ERR results
- [ ] Vtable wired up with send function pointer
- [ ] Temporary context properly managed
- [ ] All tests pass (with mock HTTP client)
- [ ] Compiles without warnings
- [ ] `make check` passes
