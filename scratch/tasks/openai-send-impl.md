# Task: OpenAI Non-Streaming Request Implementation

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** openai-request-chat.md, openai-request-responses.md, openai-response-chat.md, openai-response-responses.md, http-client.md, provider-types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

The application uses a **select()-based event loop**. ALL HTTP operations MUST be non-blocking:

- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

Reference: `src/openai/client_multi.c` (existing async pattern)

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/openai/openai.c` - Provider context and vtable
- `src/providers/openai/request.h` - Request serialization
- `src/providers/openai/response.h` - Response parsing
- `src/providers/openai/reasoning.h` - Model detection
- `src/providers/common/http_multi.h` - Async HTTP client (curl_multi wrapper)
- `src/openai/client_multi.c` - Reference for existing async pattern

**Plan:**
- `scratch/plan/README.md` - Critical async constraint
- `scratch/plan/architecture.md` - Non-streaming request flow (async)
- `scratch/plan/provider-interface.md` - start_request() specification

## Objective

Implement `ik_openai_start_request_impl()` - the vtable `start_request()` function for non-streaming requests. This function decides which API to use (Chat Completions or Responses API), serializes the request, and adds it to the curl_multi handle. **It returns immediately; the response is delivered via completion callback.**

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
| `ik_openai_start_request_impl(ctx, req, completion_cb, completion_ctx)` | Vtable start_request implementation |

### Function Signature

```c
/**
 * Start a non-streaming request (returns immediately)
 *
 * @param ctx            Provider context (ik_openai_ctx_t*)
 * @param req            Request to send
 * @param completion_cb  Callback invoked when request completes
 * @param completion_ctx User context for callback
 * @return               OK(NULL) if request started, ERR(...) on setup failure
 *
 * Response is delivered via completion_cb, NOT as a return value.
 * Caller must drive event loop with fdset/perform/info_read.
 */
res_t ik_openai_start_request_impl(void *ctx, const ik_request_t *req,
                                    ik_completion_cb_t completion_cb,
                                    void *completion_ctx);
```

### Callback Type (from provider.h)

```c
/**
 * Completion callback - invoked when request finishes
 *
 * @param completion Completion info with response data or error
 * @param ctx        User-provided context
 * @return           OK(NULL) on success, ERR(...) on failure
 */
typedef res_t (*ik_completion_cb_t)(const ik_http_completion_t *completion, void *ctx);
```

## Behaviors

### Async Request Pattern

**CRITICAL:** This function is NON-BLOCKING. It must:
1. Serialize the request to JSON
2. Configure curl easy handle
3. Add easy handle to curl_multi via `ik_http_multi_add_request()`
4. Return immediately

The actual HTTP transfer happens when the caller drives the event loop:
```c
// Caller's event loop (in REPL)
while (!done) {
    provider->vt->fdset(ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
    select(max_fd + 1, &read_fds, &write_fds, &exc_fds, &timeout);
    provider->vt->perform(ctx, &running);
    provider->vt->info_read(ctx, logger);  // completion_cb invoked here
}
```

### API Selection

- Check `ctx->use_responses_api` flag first (explicit override)
- If not set, call `ik_openai_prefer_responses_api(req->model)`
- Reasoning models (o1, o3) prefer Responses API
- Regular models (gpt-4o, gpt-5) use Chat Completions

### Request Flow (start_request)

1. Validate inputs (ctx, req, completion_cb not NULL)
2. Determine which API to use based on selection logic
3. Serialize request using appropriate serializer (chat or responses)
4. Build URL for chosen endpoint
5. Build HTTP headers with API key (Authorization: Bearer)
6. Create request context struct to hold completion callback and state
7. Call `ik_http_multi_add_request()` with:
   - URL, headers, JSON body
   - Internal write callback for response accumulation
   - Request context for tracking
8. Return OK(NULL) immediately

### Response Delivery (via info_read)

When the HTTP transfer completes (detected in `info_read()`):
1. Check HTTP status code
2. If status >= 400: parse error response with `ik_openai_parse_error()`
3. If success: parse response with appropriate parser
4. Create `ik_http_completion_t` with response or error
5. Invoke `completion_cb(completion, completion_ctx)`
6. Clean up request context

### Request Serialization

- Use `ik_openai_serialize_chat_request()` for Chat Completions
- Use `ik_openai_serialize_responses_request()` for Responses API
- Pass streaming=false for non-streaming requests
- Propagate serialization errors from start_request (ERR return)

### URL Building

- Use `ik_openai_build_chat_url()` for Chat Completions
- Use `ik_openai_build_responses_url()` for Responses API
- Pass `ctx->base_url` from provider context

### Error Handling

**Synchronous errors (from start_request):**
- Invalid arguments → return ERR(INVALID_ARG)
- Serialization failure → return ERR(...)
- curl setup failure → return ERR(...)

**Asynchronous errors (via completion_cb):**
- HTTP status >= 400 → call `ik_openai_parse_error()`, deliver via callback
- Network error → deliver error via callback
- Parse error → deliver error via callback

### Memory Management

- Request context allocated as child of provider context
- Response accumulated in request context during transfer
- Parsed response allocated on completion callback's context (not request context)
- Request context freed after completion callback returns
- Use talloc destructors for curl easy handle cleanup

### Request Context Structure

```c
// Internal structure for tracking in-flight requests
typedef struct {
    ik_openai_ctx_t *provider;    // Provider context
    bool use_responses_api;        // Which API was selected
    ik_completion_cb_t cb;         // User's completion callback
    void *cb_ctx;                  // User's callback context
    char *response_buf;            // Accumulated response data
    size_t response_len;           // Length of accumulated data
    size_t response_cap;           // Capacity of buffer
    long http_status;              // HTTP status code
} ik_openai_request_ctx_t;
```

## Test Scenarios

### API Selection - Reasoning Model

- Request with o1 model
- Verify `ik_openai_prefer_responses_api()` returns true
- start_request uses Responses API serializer

### API Selection - Chat Model

- Request with gpt-4o model
- Verify `ik_openai_prefer_responses_api()` returns false
- start_request uses Chat Completions serializer

### API Selection - Override Flag

- Provider created with use_responses_api=true
- Request with gpt-4o model
- Should use Responses API despite model preference

### Async Flow - Request Start

- Call start_request with mock HTTP multi handle
- Verify function returns immediately (OK)
- Verify request added to curl_multi handle
- Verify completion_cb NOT called yet

### Async Flow - Request Completion

- Simulate fdset/perform/info_read cycle
- Verify completion_cb invoked with response
- Verify correct parser called based on API selection

### HTTP Error Handling

- Mock HTTP returns 401 status
- Drive event loop to completion
- Verify `ik_openai_parse_error()` called
- Verify completion_cb receives error in completion struct

### Serialization Error

- Request with invalid data
- Verify start_request returns ERR (synchronous error)
- Verify completion_cb NOT called

### Parse Error (async)

- Mock HTTP returns success with malformed JSON
- Drive event loop to completion
- Verify completion_cb receives parse error

### Memory Cleanup - Success

- Complete request successfully
- Verify request context freed
- Verify response accessible in callback

### Memory Cleanup - Error

- Simulate HTTP error
- Verify request context freed
- Verify no memory leaks

## Postconditions

- [ ] `ik_openai_start_request_impl()` implemented in openai.c
- [ ] Function returns immediately (non-blocking)
- [ ] Response delivered via completion callback, not return value
- [ ] API selection logic correct for all model types
- [ ] use_responses_api flag properly respected
- [ ] Request serialization uses correct serializer
- [ ] Uses `ik_http_multi_add_request()` (NOT blocking ik_http_post)
- [ ] Response parsing uses correct parser
- [ ] HTTP errors delivered via callback
- [ ] Synchronous errors (serialization) returned from start_request
- [ ] Vtable wired up with start_request function pointer
- [ ] Request context properly managed
- [ ] All tests pass (with mock curl_multi)
- [ ] Compiles without warnings
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: openai-send-impl.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).