# Task: Create Shared HTTP Multi-Handle Client

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** provider-types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking:

- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

This is a **shared async HTTP layer** that all providers will use. The existing `src/openai/client_multi.c` is the reference implementation showing exactly how to implement this pattern.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load memory` - Talloc patterns (ownership, destructors)
- `/load errors` - Result types (OK/ERR patterns)

**Source (Reference Implementation - READ CAREFULLY):**
- `src/openai/client_multi.c` - Reference async implementation (COPY THIS PATTERN)
- `src/openai/client_multi.h` - Reference public API
- `src/openai/client_multi_internal.h` - Reference internal structures
- `src/openai/client_multi_request.c` - Reference request building
- `src/wrapper.h` - curl wrapper functions (curl_multi_init_, curl_multi_perform_, etc.)

**Plan:**
- `scratch/plan/README.md` - Critical constraint: select()-based event loop
- `scratch/plan/architecture.md` - Common HTTP layer section, async pattern
- `scratch/plan/provider-interface.md` - HTTP multi-handle creation, event loop methods

## Objective

Create `src/providers/common/http_multi.h` and `http_multi.c` - a shared async HTTP client wrapper around curl_multi that all providers will use. This generalizes the pattern from `src/openai/client_multi.c` to work with any provider.

The key difference from the OpenAI implementation:
- Generic (not OpenAI-specific) - no conversation or config dependencies
- Takes raw JSON body and headers
- Returns raw response data to caller
- Caller (provider adapter) handles serialization/deserialization

## Interface

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_http_multi_t` | CURLM *multi_handle, active_request_t **active_requests, size_t active_count | Multi-handle manager for async HTTP |
| `ik_http_request_t` | url (char*), method (char*), headers (char**), body (char*), body_len (size_t) | HTTP request to add |
| `ik_http_completion_t` | type (enum), http_code (int32), curl_code (int32), error_message (char*), response_body (char*), response_len (size_t) | Completion info for callbacks |

### Callback Types

| Type | Signature | Purpose |
|------|-----------|---------|
| `ik_http_write_cb_t` | `size_t (*)(const char *data, size_t len, void *ctx)` | Callback for streaming data as it arrives (during perform) |
| `ik_http_completion_cb_t` | `void (*)(const ik_http_completion_t *completion, void *ctx)` | Callback when request completes (from info_read) |

### Functions to Implement

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_http_multi_create` | `res_t (void *parent)` | Create multi-handle manager, returns OK(ik_http_multi_t*) |
| `ik_http_multi_fdset` | `res_t (ik_http_multi_t *multi, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd)` | Get FDs for select() |
| `ik_http_multi_perform` | `res_t (ik_http_multi_t *multi, int *still_running)` | Non-blocking I/O processing |
| `ik_http_multi_timeout` | `res_t (ik_http_multi_t *multi, long *timeout_ms)` | Get recommended timeout for select() |
| `ik_http_multi_info_read` | `void (ik_http_multi_t *multi, ik_logger_t *logger)` | Check for completed transfers, invoke callbacks |
| `ik_http_multi_add_request` | `res_t (ik_http_multi_t *multi, const ik_http_request_t *req, ik_http_write_cb_t write_cb, void *write_ctx, ik_http_completion_cb_t completion_cb, void *completion_ctx)` | Add async request to multi handle |

## Behaviors

### Multi-Handle Creation (`ik_http_multi_create`)

- Allocate `ik_http_multi_t` with `talloc_zero()`
- Initialize curl_multi handle with `curl_multi_init_()`
- Set talloc destructor for cleanup
- Return `OK(multi)` or `ERR(parent, IO, "...")` on failure

### Destructor Pattern

- Iterate active_requests and clean up each:
  - `curl_multi_remove_handle_()` to detach from multi
  - `curl_easy_cleanup_()` to destroy easy handle
  - `curl_slist_free_all_()` to free headers
  - talloc frees the request context
- Call `curl_multi_cleanup_()` on multi_handle

### fdset (`ik_http_multi_fdset`)

- Assert parameters not NULL (with `// LCOV_EXCL_BR_LINE`)
- Call `curl_multi_fdset_()` on multi_handle
- Return `OK(NULL)` on success
- Return `ERR(multi, IO, "...")` with `curl_multi_strerror_()` on failure

### perform (`ik_http_multi_perform`)

- Assert parameters not NULL
- Call `curl_multi_perform_()` on multi_handle
- Output still_running count
- Return `OK(NULL)` or `ERR(multi, IO, "...")`

### timeout (`ik_http_multi_timeout`)

- Assert parameters not NULL
- Call `curl_multi_timeout_()` on multi_handle
- Output timeout_ms
- Return `OK(NULL)` or `ERR(multi, IO, "...")`

### info_read (`ik_http_multi_info_read`)

- Loop `curl_multi_info_read_()` until returns NULL
- For each `CURLMSG_DONE`:
  - Find matching active_request by easy_handle
  - Get HTTP response code via `curl_easy_getinfo_()`
  - Build `ik_http_completion_t` with status, body, etc.
  - Invoke completion callback if provided
  - Clean up curl handles
  - Remove from active_requests array

### add_request (`ik_http_multi_add_request`)

- Allocate request context (active_request_t)
- Create curl easy handle with `curl_easy_init_()`
- Set URL, method, headers, body using `curl_easy_setopt_()`
- Configure write callback for streaming data
- Add to multi handle with `curl_multi_add_handle_()`
- Track in active_requests array
- Return `OK(NULL)` on success

### HTTP Status Categories

From reference implementation:
- `IK_HTTP_SUCCESS` - HTTP 200-299
- `IK_HTTP_CLIENT_ERROR` - HTTP 400-499
- `IK_HTTP_SERVER_ERROR` - HTTP 500-599
- `IK_HTTP_NETWORK_ERROR` - Connection failed, timeout, DNS error

## Completion Type Mapping: HTTP to Provider Layer

**Two distinct completion types exist at different abstraction layers:**

### Low-Level Type: `ik_http_completion_t`

Defined in this task (http-client.md). Used INTERNALLY by the HTTP client layer.

**Members:**
- `type` (enum) - Status category (success, client error, server error, network error)
- `http_code` (int32) - Raw HTTP status code
- `curl_code` (int32) - CURL error code
- `error_message` (char*) - CURL error string
- `response_body` (char*) - Raw response bytes
- `response_len` (size_t) - Response body length

**Purpose:** Transport-level completion info from curl_multi. No awareness of provider APIs, JSON parsing, or retry logic.

### High-Level Type: `ik_provider_completion_t`

Defined in provider-types.md. Used by REPL and application code.

**Members:**
- `success` (bool) - Whether request succeeded
- `http_status` (int) - HTTP status code
- `response` (ik_response_t*) - Parsed provider response with content blocks
- `error_category` (ik_error_category_t) - Provider error category (auth, rate limit, etc.)
- `error_message` (char*) - Human-readable error
- `retry_after_ms` (int32_t) - Suggested retry delay

**Purpose:** Provider-level completion with parsed responses, error categorization, and retry guidance.

### Conversion Pattern: Provider Implementation Responsibility

**Provider adapters (openai, anthropic, google) bridge the two layers:**

1. **HTTP client delivers `ik_http_completion_t` to provider callback:**
   - HTTP client calls provider's internal completion callback with raw HTTP data
   - Provider callback receives `ik_http_completion_t*` with HTTP status, response body, CURL codes

2. **Provider parses and categorizes:**
   - Extract HTTP status code from `http_code`
   - Parse JSON response body into provider-specific structures
   - Map HTTP status + provider error codes to `ik_error_category_t`
   - Construct `ik_response_t*` with content blocks, usage, finish reason

3. **Provider constructs `ik_provider_completion_t`:**
   - Set `success` based on HTTP status and parse result
   - Copy `http_status` from `http_code`
   - Set `response` to parsed `ik_response_t*` (or NULL on error)
   - Map provider-specific errors to `error_category` (AUTH, RATE_LIMIT, etc.)
   - Extract retry-after header into `retry_after_ms`

4. **Provider invokes user callback with `ik_provider_completion_t`:**
   - User's completion callback receives high-level completion struct
   - User code works with parsed responses, not raw HTTP

### Pseudo-code Example

```c
// Provider's internal HTTP completion handler
static void http_completion_handler(const ik_http_completion_t *http_comp, void *ctx) {
    provider_request_ctx_t *req_ctx = ctx;

    // Construct high-level completion
    ik_provider_completion_t provider_comp = {0};
    provider_comp.http_status = http_comp->http_code;

    if (http_comp->type == IK_HTTP_SUCCESS) {
        // Parse JSON response body
        ik_response_t *response = parse_provider_response(
            http_comp->response_body,
            http_comp->response_len
        );

        if (response) {
            provider_comp.success = true;
            provider_comp.response = response;
        } else {
            provider_comp.success = false;
            provider_comp.error_category = IK_ERR_CAT_SERVER;  // Parse failure
            provider_comp.error_message = "Failed to parse response";
        }
    } else {
        // HTTP error - categorize and construct error message
        provider_comp.success = false;
        provider_comp.error_category = categorize_http_error(http_comp->http_code);
        provider_comp.error_message = http_comp->error_message;

        // Extract retry-after from headers if rate limited
        if (http_comp->http_code == 429) {
            provider_comp.retry_after_ms = extract_retry_after(req_ctx);
        }
    }

    // Invoke user's completion callback with provider-level completion
    req_ctx->user_completion_cb(&provider_comp, req_ctx->user_ctx);
}
```

### Abstraction Boundary Summary

| Layer | Type | Responsibility |
|-------|------|----------------|
| HTTP Client | `ik_http_completion_t` | Transport HTTP responses, report CURL errors |
| Provider Adapter | Both types | Convert from HTTP completion to provider completion |
| REPL/Application | `ik_provider_completion_t` | Work with parsed responses and semantic errors |

**Key principle:** The HTTP client is protocol-agnostic. Provider adapters own the mapping from HTTP semantics to provider API semantics.

### Memory Management

- Multi-handle and all children allocated via talloc
- Request bodies must persist until completion (talloc_steal)
- Responses owned by caller context
- Destructor handles all cleanup via talloc hierarchy
- CURL handles cleaned up via destructor

### Error Handling

- CURL errors mapped to `ERR(ctx, IO, "...")` with strerror
- Include curl error message in error string
- Non-200 status codes returned in completion (not errors)
- Network failures indicated by `IK_HTTP_NETWORK_ERROR` type

## Directory Structure

```
src/providers/common/
    http_multi.h
    http_multi.c

tests/unit/providers/common/
    http_multi_test.c
```

## Test Scenarios

Create `tests/unit/providers/common/http_multi_test.c`:

**Lifecycle tests (testable without network):**
- Multi-handle creation: Successfully create multi handle
- Multi-handle cleanup: Talloc destructor cleans up curl handles without crash
- fdset on empty multi: Returns OK with no FDs set
- perform on empty multi: Returns OK with still_running=0
- timeout on empty multi: Returns OK with valid timeout
- info_read on empty multi: Does nothing, no crash

**Request setup tests (validate configuration, no actual HTTP):**
- Add request: Creates curl easy handle, sets options correctly
- Add request with headers: Headers applied to easy handle
- Add request with body: Body attached correctly

**Memory lifecycle tests:**
- Parent talloc context frees all allocations
- Request context survives until completion callback
- Destructor handles active requests during cleanup

**Note:** Integration tests requiring actual HTTP servers belong in `tests/integration/`.

## Makefile Updates

Add to Makefile:
- `src/providers/common/http_multi.c` to SOURCES
- `src/providers/common/http_multi.h` to HEADERS
- `tests/unit/providers/common/http_multi_test.c` to TEST_SOURCES
- Create directories if needed: `src/providers/common/` and `tests/unit/providers/common/`

## Postconditions

- [ ] `src/providers/common/http_multi.h` exists with async API
- [ ] `src/providers/common/http_multi.c` implements curl_multi wrapper
- [ ] Directory `src/providers/common/` created
- [ ] All six event loop functions implemented (create, fdset, perform, timeout, info_read, add_request)
- [ ] Uses curl_multi (NOT curl_easy_perform) for all operations
- [ ] Talloc destructor handles cleanup
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] Compiles without warnings
- [ ] Changes committed to git with message: `task: http-client.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
