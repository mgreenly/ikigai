# Provider Interface Specification

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations
MUST be non-blocking:

- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

Reference: `src/openai/client_multi.c`

## Overview

This document specifies the vtable interface that all provider adapters must implement, along with lifecycle management and common utilities.

## Vtable Interface

### Purpose
The provider vtable defines a uniform interface for interacting with different LLM providers (Anthropic, OpenAI, Google).

### Function Pointers (Async Model)

All vtable methods are non-blocking. Request initiation returns immediately;
progress is made through the event loop integration methods.

```c
/**
 * Provider vtable for async/non-blocking HTTP operations
 *
 * All providers MUST implement these methods to integrate with the
 * select()-based event loop. Blocking implementations are NOT acceptable.
 */
struct ik_provider_vtable_t {
    /* ============================================================
     * Event Loop Integration (REQUIRED)
     * These methods integrate the provider with select()
     * ============================================================ */

    /**
     * fdset - Populate fd_sets for select()
     *
     * @param ctx       Provider context (opaque)
     * @param read_fds  Read fd_set to populate (will be modified)
     * @param write_fds Write fd_set to populate (will be modified)
     * @param exc_fds   Exception fd_set to populate (will be modified)
     * @param max_fd    Output: highest FD number (will be updated)
     * @return          OK(NULL) on success, ERR(...) on failure
     *
     * Called before select() to get file descriptors the provider
     * needs to monitor. Provider adds its curl_multi FDs to the sets.
     */
    res_t (*fdset)(void *ctx, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd);

    /**
     * perform - Process pending I/O operations
     *
     * @param ctx             Provider context (opaque)
     * @param running_handles Output: number of active transfers
     * @return                OK(NULL) on success, ERR(...) on failure
     *
     * Called after select() returns to process ready file descriptors.
     * This drives curl_multi_perform() internally. Non-blocking.
     */
    res_t (*perform)(void *ctx, int *running_handles);

    /**
     * timeout - Get recommended timeout for select()
     *
     * @param ctx        Provider context (opaque)
     * @param timeout_ms Output: recommended timeout in milliseconds
     * @return           OK(NULL) on success, ERR(...) on failure
     *
     * Returns curl's recommended timeout. Caller should use minimum
     * of this and any other timeout requirements.
     */
    res_t (*timeout)(void *ctx, long *timeout_ms);

    /**
     * info_read - Process completed transfers
     *
     * @param ctx    Provider context (opaque)
     * @param logger Logger for debug output
     *
     * Called after perform() to check for completed transfers.
     * Invokes completion callbacks for finished requests.
     */
    void (*info_read)(void *ctx, ik_logger_t *logger);

    /* ============================================================
     * Request Initiation (Non-blocking)
     * These methods start requests but return immediately
     * ============================================================ */

    /**
     * start_request - Initiate non-streaming request
     *
     * @param ctx           Provider context
     * @param req           Request to send
     * @param completion_cb Callback invoked when request completes
     * @param completion_ctx User context for callback
     * @return              OK(NULL) on success, ERR(...) on failure
     *
     * Returns immediately. Request progresses through perform().
     * completion_cb is invoked from info_read() when transfer completes.
     */
    res_t (*start_request)(void *ctx, const ik_request_t *req,
                           ik_completion_cb_t completion_cb, void *completion_ctx);

    /**
     * start_stream - Initiate streaming request
     *
     * @param ctx           Provider context
     * @param req           Request to send
     * @param stream_cb     Callback for streaming events
     * @param stream_ctx    User context for stream callback
     * @param completion_cb Callback invoked when stream completes
     * @param completion_ctx User context for completion callback
     * @return              OK(NULL) on success, ERR(...) on failure
     *
     * Returns immediately. Stream events delivered via stream_cb
     * as data arrives during perform(). completion_cb invoked when done.
     */
    res_t (*start_stream)(void *ctx, const ik_request_t *req,
                          ik_stream_cb_t stream_cb, void *stream_ctx,
                          ik_completion_cb_t completion_cb, void *completion_ctx);

    /* ============================================================
     * Cleanup
     * ============================================================ */

    /**
     * cleanup - Release provider resources
     *
     * @param ctx Provider context
     *
     * Optional if talloc hierarchy handles all cleanup.
     * Called before provider is freed.
     */
    void (*cleanup)(void *ctx);
};
```

### Callback Signatures

```c
/**
 * Stream callback - receives streaming events as data arrives
 *
 * @param event Stream event (text delta, tool call, etc.)
 * @param ctx   User-provided context
 * @return      OK(NULL) to continue, ERR(...) to abort stream
 */
typedef res_t (*ik_stream_cb_t)(const ik_stream_event_t *event, void *ctx);

/**
 * Completion callback - invoked when request finishes
 *
 * @param completion Completion info (success/error, usage, etc.)
 * @param ctx        User-provided context
 * @return           OK(NULL) on success, ERR(...) on failure
 */
typedef res_t (*ik_completion_cb_t)(const ik_http_completion_t *completion, void *ctx);
```

See [streaming.md](streaming.md) for stream event structure details.

## Response Handling

Responses are delivered exclusively via callbacks passed to `start_request`/`start_stream`:

```c
res_t (*start_request)(void *ctx, const ik_request_t *req,
                       ik_completion_cb_t on_complete, void *user_data);
res_t (*start_stream)(void *ctx, const ik_request_t *req,
                      ik_stream_cb_t on_chunk, void *stream_ctx,
                      ik_completion_cb_t on_complete, void *user_data);
```

**Callback invocation:**
- Stream callbacks (`on_chunk`) are invoked during `perform()` as data arrives
- Completion callbacks (`on_complete`) are invoked from `info_read()` when the transfer finishes

**Usage pattern:**
1. Register callbacks when starting a request
2. Drive the event loop (fdset/select/perform/info_read cycle)
3. Callbacks fire automatically as data arrives and when complete

This callback-only design keeps the interface simple and matches the
select()-based event loop architecture.

## Provider Context

### Purpose
Each provider maintains its own context structure containing:
- Credentials (API key)
- Shared HTTP client reference
- Provider-specific configuration (thinking budgets, rate limits, etc.)
- Optional rate limit tracking state

### Ownership
Context is talloc-allocated and owned by the provider wrapper. Cleaned up when provider is freed.

## Factory Functions

### Provider Creation
Each provider module exports a factory function with signature:
- Function name: `ik_{provider}_create()`
- Parameters: talloc context, API key, output provider pointer
- Returns: result type (OK/ERR)
- Responsibility: initialize provider context and vtable

Providers supported:
- Anthropic: `ik_anthropic_create()`
- OpenAI: `ik_openai_create()`
- Google: `ik_google_create()`

## Implementation Requirements

### Event Loop Methods (REQUIRED)

**fdset() Requirements:**
- Call `curl_multi_fdset()` on provider's multi handle
- Add provider's FDs to the provided fd_sets
- Update max_fd if provider has higher FDs
- Must be non-blocking (no I/O operations)

**perform() Requirements:**
- Call `curl_multi_perform()` on provider's multi handle
- Return number of still-running handles via output parameter
- Must be non-blocking - returns immediately
- Stream callbacks invoked during this call as data arrives

**timeout() Requirements:**
- Call `curl_multi_timeout()` on provider's multi handle
- Return recommended timeout in milliseconds
- Caller uses minimum of all provider timeouts

**info_read() Requirements:**
- Call `curl_multi_info_read()` to check for completed transfers
- Invoke completion callbacks for finished requests
- Parse final response and populate ik_response_t
- Clean up completed transfer resources

### start_request() Requirements
- Transform internal request to provider wire format (JSON)
- Configure curl easy handle with POST request
- Add easy handle to provider's multi handle
- Register completion callback
- Return immediately (non-blocking)
- Actual I/O happens during perform() calls

### start_stream() Requirements
- Transform internal request to provider wire format with stream flag
- Configure curl easy handle with streaming write callback
- Write callback invokes SSE parser as data arrives
- SSE parser invokes user's stream callback with normalized events
- Add easy handle to provider's multi handle
- Return immediately (non-blocking)

### cleanup() Requirements
- Optional implementation
- Only needed for resources beyond talloc hierarchy
- Most providers can set cleanup = NULL

## Error Handling Contract

### Error Categories
Providers must map HTTP and API errors to these categories:
- ERR_AUTH - Invalid credentials (401)
- ERR_RATE_LIMIT - Rate limit exceeded (429)
- ERR_INVALID_ARG - Bad request (400)
- ERR_NOT_FOUND - Model not found (404)
- ERR_SERVER - Server error (500, 502, 503)
- ERR_TIMEOUT - Request timeout
- ERR_CONTENT_FILTER - Content policy violation
- ERR_NETWORK - Network/connection error
- ERR_UNKNOWN - Other errors

See [error-handling.md](error-handling.md) for full mapping tables.

### Error Information
Error structures include:
- Category enum
- HTTP status code (0 if not HTTP error)
- Human-readable message
- Provider's error code/type
- Retry-after milliseconds (-1 if not applicable)

## Common Utilities

### Credentials Loading
**ik_credentials_load()** - Load API key for provider
- Checks environment variable first ({PROVIDER}_API_KEY)
- Falls back to credentials.json file
- Returns allocated key string on success

### Environment Variable Names
**ik_provider_env_var()** - Get environment variable name for provider
- Maps provider name to environment variable
- Examples: "anthropic" → "ANTHROPIC_API_KEY", "openai" → "OPENAI_API_KEY"
- Returns static string (no allocation)

Supported mappings:
- anthropic → ANTHROPIC_API_KEY
- openai → OPENAI_API_KEY
- google → GOOGLE_API_KEY
- xai → XAI_API_KEY
- meta → LLAMA_API_KEY

### HTTP Multi-Handle Creation
**ik_http_multi_create()** - Create curl_multi handle for provider
- Parameters: talloc context, output multi pointer
- Returns: result type
- **MUST use curl_multi, NOT curl_easy**
- Multi handle manages all async transfers for the provider
- Integrates with select() via fdset/perform pattern

```c
/**
 * Create HTTP multi-handle for async operations
 *
 * @param ctx    Talloc context
 * @param out    Output: multi-handle manager
 * @return       OK(multi) or ERR(...)
 *
 * The returned handle wraps curl_multi and provides:
 * - fdset() for select() integration
 * - perform() for non-blocking I/O
 * - timeout() for select() timeout calculation
 * - info_read() for completion handling
 */
res_t ik_http_multi_create(TALLOC_CTX *ctx, ik_http_multi_t **out);
```

### SSE Parser Creation
**ik_sse_parser_create()** - Create SSE parser
- Parameters: talloc context, event callback, user context, output parser pointer
- Returns: result type
- Parser handles SSE protocol and event extraction
- Invoked from curl write callback during perform()

## Provider Registration

### Dispatch Mechanism
Static dispatch in `ik_provider_create()`:
- Loads credentials via `ik_credentials_load()`
- Dispatches to appropriate factory based on provider name
- Returns error for unknown providers
- No dynamic registration - simple switch/if-else

## Thread Safety

Not required for rel-07. ikigai is single-threaded. Providers can assume single-threaded access.

## Performance Considerations

### Connection Pooling
HTTP clients may implement connection pooling for reusing connections across multiple requests.

### Request Caching
Not implemented in rel-07. Future optimization.

### Partial Response Accumulation
Streaming implementations should accumulate partial responses efficiently:
- Text deltas accumulated in string builder
- Tool calls tracked in array
- Token usage accumulated

## Extension Points

### Adding New Providers
Steps to add provider support (xAI, Meta, etc.):
1. Create `src/providers/{name}/` directory
2. Implement factory function: `ik_{name}_create()`
3. Implement vtable functions (send, stream, cleanup)
4. Add dispatch case to `ik_provider_create()`
5. Add credential loading for `{NAME}_API_KEY`
6. Add tests in `tests/unit/providers/`

### Custom Headers
Providers can add custom headers as needed:
- Anthropic requires: `x-api-key`, `anthropic-version`
- OpenAI supports optional: `OpenAI-Organization`, `OpenAI-Project`

### Provider-Specific Features
Features only supported by some providers can be added to `provider_data` field in request/response structures. Providers extract during serialization while keeping core format clean.
