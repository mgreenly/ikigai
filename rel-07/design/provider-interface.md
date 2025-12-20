# Provider Interface Specification

## Overview

This document specifies the vtable interface that all provider adapters must implement, along with lifecycle management and common utilities.

## Vtable Definition

```c
// src/providers/provider.h

typedef struct ik_provider_vtable {
    /**
     * Send non-streaming request to provider.
     *
     * @param impl_ctx Provider-specific context (opaque)
     * @param req Internal request format
     * @param out_resp Response (allocated by callee)
     * @return OK on success, ERR on failure
     */
    res_t (*send)(void *impl_ctx,
                  ik_request_t *req,
                  ik_response_t **out_resp);

    /**
     * Send streaming request to provider.
     *
     * @param impl_ctx Provider-specific context (opaque)
     * @param req Internal request format
     * @param cb Callback for streaming events (normalized)
     * @param cb_ctx User context passed to callback
     * @return OK on success, ERR on failure
     */
    res_t (*stream)(void *impl_ctx,
                    ik_request_t *req,
                    ik_stream_callback_t cb,
                    void *cb_ctx);

    /**
     * Cleanup provider-specific resources.
     *
     * Called when provider is being freed (via talloc destructor).
     * Optional if talloc hierarchy handles all cleanup.
     *
     * @param impl_ctx Provider-specific context
     */
    void (*cleanup)(void *impl_ctx);

} ik_provider_vtable_t;
```

## Stream Callback

```c
typedef void (*ik_stream_callback_t)(ik_stream_event_t *event,
                                      void *user_ctx);
```

See [streaming.md](streaming.md) for `ik_stream_event_t` definition.

## Provider Context

Each provider defines its own context structure:

```c
// Example: src/providers/anthropic/anthropic.h

typedef struct ik_anthropic_ctx {
    char *api_key;                // Credentials
    ik_http_client_t *http;       // Shared HTTP client

    // Provider-specific state
    int32_t thinking_min;         // Minimum thinking budget
    int32_t thinking_max;         // Maximum thinking budget

    // Rate limit tracking (optional)
    int32_t requests_remaining;
    int32_t tokens_remaining;

} ik_anthropic_ctx_t;
```

**Ownership:** Context is talloc-allocated and owned by `ik_provider_t`. Cleaned up when provider is freed.

## Factory Functions

Each provider module exports a factory function:

```c
// src/providers/anthropic/anthropic.h

/**
 * Create Anthropic provider instance.
 *
 * @param ctx Talloc context
 * @param api_key Anthropic API key (will be copied)
 * @param out_provider Created provider
 * @return OK on success, ERR on failure
 */
res_t ik_anthropic_create(TALLOC_CTX *ctx,
                          const char *api_key,
                          ik_provider_t **out_provider);
```

Similar signatures for `ik_openai_create()` and `ik_google_create()`.

## Implementation Requirements

### send() Implementation

**Responsibilities:**
1. Transform `ik_request_t` to provider wire format (JSON)
2. Make HTTP POST request to provider API
3. Handle HTTP errors (401, 429, 500, etc.)
4. Parse response JSON to `ik_response_t`
5. Map provider finish reasons to `ik_finish_reason_t`
6. Extract token usage
7. Return errors with appropriate category

**Example:**

```c
static res_t ik_anthropic_send(void *impl_ctx,
                               ik_request_t *req,
                               ik_response_t **out_resp)
{
    ik_anthropic_ctx_t *ctx = impl_ctx;
    TALLOC_CTX *tmp = talloc_new_(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // 1. Serialize request
    char *json = NULL;
    TRY_CLEAN(ik_anthropic_serialize_request(tmp, req, &json), tmp);

    // 2. Build headers
    const char *headers[] = {
        "Content-Type: application/json",
        talloc_asprintf(tmp, "x-api-key: %s", ctx->api_key),
        "anthropic-version: 2023-06-01",
        NULL
    };

    // 3. Make HTTP request
    char *response = NULL;
    int status = 0;
    res_t http_result = ik_http_post(ctx->http, "/v1/messages", headers,
                                     json, &response, &status);
    if (is_err(&http_result)) {
        talloc_free(tmp);
        return http_result;
    }

    talloc_steal(tmp, response);

    // 4. Handle HTTP errors
    if (status != 200) {
        res_t err_result = ik_anthropic_handle_error(tmp, status, response);
        talloc_free(tmp);
        return err_result;
    }

    // 5. Parse response
    ik_response_t *resp = NULL;
    TRY_CLEAN(ik_anthropic_parse_response(tmp, response, &resp), tmp);

    // 6. Return response (steal to parent context)
    talloc_steal(*out_resp, resp);
    *out_resp = resp;
    talloc_free(tmp);
    return OK(NULL);
}
```

### stream() Implementation

**Responsibilities:**
1. Transform `ik_request_t` to provider wire format
2. Make HTTP POST request with SSE streaming
3. Parse SSE events from provider
4. Convert provider events to normalized `ik_stream_event_t`
5. Call user callback with normalized events
6. Handle mid-stream errors

**Example:**

```c
// Stream context passed to SSE callback
typedef struct {
    ik_stream_callback_t user_cb;
    void *user_ctx;
    ik_response_t *partial_response;  // Accumulated response
} ik_anthropic_stream_ctx_t;

// SSE callback (raw events from provider)
static void anthropic_on_sse_event(const char *event,
                                   const char *data,
                                   void *user_ctx)
{
    ik_anthropic_stream_ctx_t *stream_ctx = user_ctx;

    // Parse provider event
    if (strcmp(event, "content_block_delta") == 0) {
        // Parse JSON data
        yyjson_doc *doc = yyjson_read(data, strlen(data), 0);
        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *delta = yyjson_obj_get(root, "delta");
        const char *type = yyjson_get_str(yyjson_obj_get(delta, "type"));

        if (strcmp(type, "text_delta") == 0) {
            // Text content delta
            const char *text = yyjson_get_str(yyjson_obj_get(delta, "text"));

            // Create normalized event
            ik_stream_event_t norm_event = {
                .type = IK_STREAM_TEXT_DELTA,
                .data.text_delta.text = text
            };

            // Call user callback
            stream_ctx->user_cb(&norm_event, stream_ctx->user_ctx);
        }
        else if (strcmp(type, "thinking_delta") == 0) {
            // Thinking content delta
            const char *text = yyjson_get_str(yyjson_obj_get(delta, "text"));

            ik_stream_event_t norm_event = {
                .type = IK_STREAM_THINKING_DELTA,
                .data.thinking_delta.text = text
            };

            stream_ctx->user_cb(&norm_event, stream_ctx->user_ctx);
        }

        yyjson_doc_free(doc);
    }
    else if (strcmp(event, "message_stop") == 0) {
        // Final event
        ik_stream_event_t norm_event = {
            .type = IK_STREAM_DONE,
            .data.done = {
                .finish_reason = IK_FINISH_STOP,
                .usage = stream_ctx->partial_response->usage
            }
        };

        stream_ctx->user_cb(&norm_event, stream_ctx->user_ctx);
    }
}

static res_t ik_anthropic_stream(void *impl_ctx,
                                 ik_request_t *req,
                                 ik_stream_callback_t cb,
                                 void *cb_ctx)
{
    ik_anthropic_ctx_t *ctx = impl_ctx;
    TALLOC_CTX *tmp = talloc_new_(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create stream context
    ik_anthropic_stream_ctx_t stream_ctx = {
        .user_cb = cb,
        .user_ctx = cb_ctx,
        .partial_response = talloc_zero_(tmp, sizeof(ik_response_t))
    };

    // Serialize request (add "stream": true)
    char *json = NULL;
    TRY_CLEAN(ik_anthropic_serialize_request(tmp, req, &json), tmp);

    // Build headers
    const char *headers[] = {
        "Content-Type: application/json",
        talloc_asprintf(tmp, "x-api-key: %s", ctx->api_key),
        "anthropic-version: 2023-06-01",
        NULL
    };

    // Make streaming request
    res_t result = ik_http_post_stream(ctx->http, "/v1/messages", headers,
                                       json, anthropic_on_sse_event,
                                       &stream_ctx);

    talloc_free(tmp);
    return result;
}
```

### cleanup() Implementation

**Optional.** Only needed if provider maintains state beyond talloc hierarchy.

```c
static void ik_anthropic_cleanup(void *impl_ctx)
{
    ik_anthropic_ctx_t *ctx = impl_ctx;

    // Example: close persistent connections
    if (ctx->persistent_connection) {
        close_connection(ctx->persistent_connection);
    }

    // Talloc handles memory cleanup automatically
}
```

Most providers can set `cleanup = NULL` if talloc handles everything.

## Error Handling Contract

### Error Categories

Providers must map errors to these categories:

```c
typedef enum {
    ERR_AUTH,           // 401 - Invalid credentials
    ERR_RATE_LIMIT,     // 429 - Rate limit exceeded
    ERR_INVALID_ARG,    // 400 - Bad request
    ERR_NOT_FOUND,      // 404 - Model not found
    ERR_SERVER,         // 500, 502, 503 - Server error
    ERR_TIMEOUT,        // Request timeout
    ERR_CONTENT_FILTER, // Content policy violation
    ERR_NETWORK,        // Network/connection error
    ERR_UNKNOWN         // Other errors
} ik_error_category_t;
```

See [error-handling.md](error-handling.md) for full mapping tables.

### Error Structure

```c
typedef struct {
    ik_error_category_t category;
    int http_status;             // HTTP status code (0 if not HTTP error)
    char *message;               // Human-readable message
    char *provider_code;         // Provider's error type (e.g., "rate_limit_error")
    int retry_after_ms;          // Milliseconds to wait before retry (-1 if not applicable)
} ik_error_t;
```

**Usage:**

```c
if (status == 429) {
    // Extract retry-after header
    int retry_after = parse_retry_after_header(response_headers);

    return ERR_DETAILED(ctx, ERR_RATE_LIMIT, status, "rate_limit_error",
                       retry_after * 1000,
                       "Rate limit exceeded. Retry after %d seconds", retry_after);
}
```

## Common Utilities

### Credentials Loading

```c
// src/providers/provider_common.c

/**
 * Load API key for provider from environment or credentials.json.
 *
 * Checks (in order):
 * 1. Environment variable ({PROVIDER}_API_KEY)
 * 2. credentials.json file
 *
 * @param ctx Talloc context
 * @param provider_name Provider name ("anthropic", "openai", etc.)
 * @param out_key API key (allocated on ctx)
 * @return OK if found, ERR if not found
 */
res_t ik_credentials_load(TALLOC_CTX *ctx,
                          const char *provider_name,
                          char **out_key);
```

### Environment Variable Names

```c
/**
 * Get environment variable name for provider.
 *
 * @param provider_name "anthropic" → "ANTHROPIC_API_KEY"
 * @return Static string (no allocation)
 */
const char *ik_provider_env_var(const char *provider_name);
```

Maps:
- `"anthropic"` → `"ANTHROPIC_API_KEY"`
- `"openai"` → `"OPENAI_API_KEY"`
- `"google"` → `"GOOGLE_API_KEY"`
- `"xai"` → `"XAI_API_KEY"`
- `"meta"` → `"LLAMA_API_KEY"`

### HTTP Client Creation

```c
// src/providers/common/http_client.h

/**
 * Create HTTP client for provider base URL.
 *
 * @param ctx Talloc context
 * @param base_url Provider API base URL
 * @param out_client Created client
 * @return OK on success
 */
res_t ik_http_client_create(TALLOC_CTX *ctx,
                            const char *base_url,
                            ik_http_client_t **out_client);
```

### SSE Parser Creation

```c
// src/providers/common/sse_parser.h

/**
 * Create SSE parser.
 *
 * @param ctx Talloc context
 * @param cb Callback for SSE events
 * @param user_ctx User context passed to callback
 * @param out_parser Created parser
 * @return OK on success
 */
res_t ik_sse_parser_create(TALLOC_CTX *ctx,
                           ik_sse_callback_t cb,
                           void *user_ctx,
                           ik_sse_parser_t **out_parser);
```

## Provider Registration

Providers are registered via simple dispatch:

```c
// src/providers/provider_common.c

res_t ik_provider_create(TALLOC_CTX *ctx,
                         const char *name,
                         ik_provider_t **out_provider)
{
    // Load credentials
    char *api_key = NULL;
    TRY(ik_credentials_load(ctx, name, &api_key));

    // Dispatch to factory
    if (strcmp(name, "anthropic") == 0) {
        return ik_anthropic_create(ctx, api_key, out_provider);
    }
    else if (strcmp(name, "openai") == 0) {
        return ik_openai_create(ctx, api_key, out_provider);
    }
    else if (strcmp(name, "google") == 0) {
        return ik_google_create(ctx, api_key, out_provider);
    }

    return ERR(ctx, ERR_INVALID_ARG, "Unknown provider: %s", name);
}
```

No dynamic registration - static dispatch keeps it simple.

## Thread Safety

**Not required for rel-07.** ikigai is single-threaded. Providers can assume single-threaded access.

## Performance Considerations

### Connection Pooling

HTTP clients may implement connection pooling for performance:

```c
ik_http_client_t *http = ...;
// Reuses connection across multiple requests
ik_http_post(http, "/v1/messages", ...);  // Opens connection
ik_http_post(http, "/v1/messages", ...);  // Reuses connection
```

### Request Caching

Not implemented in rel-07. Future optimization.

### Partial Response Accumulation

Streaming implementations should accumulate partial responses efficiently:

```c
typedef struct {
    // Accumulate text deltas
    talloc_string_builder_t *text_builder;

    // Track tool calls
    ik_content_block_t *tool_calls;
    size_t tool_call_count;

} ik_partial_response_t;
```

## Extension Points

### Future Provider Support

Adding new providers (xAI, Meta, etc.):

1. Create `src/providers/{name}/` directory
2. Implement factory function: `ik_{name}_create()`
3. Implement vtable functions
4. Add dispatch case to `ik_provider_create()`
5. Add credential loading for `{NAME}_API_KEY`
6. Add tests in `tests/unit/providers/`

### Custom Headers

Providers can add custom headers as needed:

```c
// Anthropic requires special header
headers[] = {
    "x-api-key: ...",
    "anthropic-version: 2023-06-01",  // Required
    NULL
};

// OpenAI supports organization/project headers (optional)
headers[] = {
    "Authorization: Bearer ...",
    "OpenAI-Organization: org-...",   // Optional
    "OpenAI-Project: proj-...",       // Optional
    NULL
};
```

### Provider-Specific Features

Features only supported by some providers can be added to `provider_data` in request/response:

```c
// Example: OpenAI seed parameter (reproducible outputs)
req->provider_data = yyjson_mut_obj(doc);
yyjson_mut_obj_add_int(doc, req->provider_data, "seed", 12345);

// Adapter extracts during serialization
int seed = yyjson_get_int(yyjson_obj_get(req->provider_data, "seed"));
```

This keeps the core format clean while allowing provider experimentation.
