# Provider Abstraction Architecture

## Overview

The multi-provider architecture uses a **vtable pattern** to abstract AI provider APIs behind a unified interface. Each provider implements the same vtable, enabling polymorphic behavior without tight coupling.

## Directory Structure

```
src/
  providers/
    common/
      http_client.c        # Shared libcurl wrapper
      http_client.h
      sse_parser.c         # Shared SSE streaming parser
      sse_parser.h
    provider.h             # Vtable definition, shared types
    provider_common.c      # Utility functions

    anthropic/
      adapter.c            # Vtable implementation
      client.c             # API client
      streaming.c          # SSE event handlers
      anthropic.h          # Public interface

    openai/
      adapter.c            # Refactored from src/openai/*
      client.c
      streaming.c
      openai.h

    google/
      adapter.c
      client.c
      streaming.c
      google.h
```

## Vtable Pattern

### Core Types

```c
// src/providers/provider.h

typedef struct ik_provider_vtable {
    // Send non-streaming request
    res_t (*send)(void *impl_ctx,
                  ik_request_t *req,
                  ik_response_t **out_resp);

    // Send streaming request
    res_t (*stream)(void *impl_ctx,
                    ik_request_t *req,
                    ik_stream_callback_t cb,
                    void *cb_ctx);

    // Cleanup provider-specific resources
    void (*cleanup)(void *impl_ctx);
} ik_provider_vtable_t;

typedef struct ik_provider {
    const char *name;              // "anthropic", "openai", "google"
    ik_provider_vtable_t *vt;      // Function pointers
    void *impl_ctx;                // Provider-specific context
} ik_provider_t;
```

### Provider Context

Each provider maintains its own context structure:

```c
// src/providers/anthropic/anthropic.h

typedef struct ik_anthropic_ctx {
    char *api_key;                 // Loaded from env or credentials.json
    ik_http_client_t *http;        // Shared HTTP client
    // Provider-specific state
} ik_anthropic_ctx_t;
```

## Provider Lifecycle

### Lazy Initialization

Providers are created on first use:

```c
// Agent sets provider/model
agent->provider = "anthropic";
agent->model = "claude-sonnet-4-5";
agent->thinking_level = IK_THINKING_MED;

// First message send triggers provider creation
res_t result = ik_provider_get_or_create(agent->provider, &provider);
if (is_err(&result)) {
    // "No credentials for anthropic. Set ANTHROPIC_API_KEY..."
}
```

### Creation Flow

```c
res_t ik_provider_create(TALLOC_CTX *ctx,
                         const char *name,
                         ik_provider_t **out_provider)
{
    // 1. Load credentials (env var or credentials.json)
    char *api_key = NULL;
    res_t cred_result = ik_credentials_load(ctx, name, &api_key);
    if (is_err(&cred_result)) {
        return ERR(ctx, ERR_AUTH,
                   "No credentials for %s. Set %s_API_KEY or add to credentials.json",
                   name, ik_provider_env_var(name));
    }

    // 2. Dispatch to provider-specific factory
    if (strcmp(name, "anthropic") == 0) {
        return ik_anthropic_create(ctx, api_key, out_provider);
    } else if (strcmp(name, "openai") == 0) {
        return ik_openai_create(ctx, api_key, out_provider);
    } else if (strcmp(name, "google") == 0) {
        return ik_google_create(ctx, api_key, out_provider);
    }

    return ERR(ctx, ERR_INVALID_ARG, "Unknown provider: %s", name);
}
```

### Provider Factory

Each provider implements a factory function:

```c
// src/providers/anthropic/adapter.c

res_t ik_anthropic_create(TALLOC_CTX *ctx,
                          const char *api_key,
                          ik_provider_t **out_provider)
{
    // Create provider-specific context
    ik_anthropic_ctx_t *impl = talloc_zero_(ctx, sizeof(*impl));
    if (impl == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    impl->api_key = talloc_strdup(impl, api_key);

    // Create shared HTTP client
    ik_http_client_t *http = NULL;
    TRY(ik_http_client_create(impl, "https://api.anthropic.com", &http));
    impl->http = http;

    // Create provider wrapper
    ik_provider_t *provider = talloc_zero_(ctx, sizeof(*provider));
    if (provider == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    talloc_steal(provider, impl);

    provider->name = "anthropic";
    provider->vt = &ik_anthropic_vtable;  // Static vtable
    provider->impl_ctx = impl;

    *out_provider = provider;
    return OK(NULL);
}

// Static vtable (defined once)
static ik_provider_vtable_t ik_anthropic_vtable = {
    .send = ik_anthropic_send,
    .stream = ik_anthropic_stream,
    .cleanup = ik_anthropic_cleanup
};
```

## Shared Utilities

### HTTP Client

Common HTTP functionality shared across providers:

```c
// src/providers/common/http_client.h

typedef struct ik_http_client ik_http_client_t;

// Create HTTP client for a base URL
res_t ik_http_client_create(TALLOC_CTX *ctx,
                             const char *base_url,
                             ik_http_client_t **out_client);

// POST request (non-streaming)
res_t ik_http_post(ik_http_client_t *client,
                   const char *path,
                   const char **headers,  // NULL-terminated array
                   const char *body,
                   char **out_response,
                   int *out_status);

// POST request (streaming SSE)
res_t ik_http_post_stream(ik_http_client_t *client,
                          const char *path,
                          const char **headers,
                          const char *body,
                          ik_sse_callback_t sse_cb,
                          void *cb_ctx);
```

### SSE Parser

Shared Server-Sent Events parser:

```c
// src/providers/common/sse_parser.h

typedef void (*ik_sse_callback_t)(const char *event,
                                   const char *data,
                                   void *user_ctx);

// Parse SSE stream
res_t ik_sse_parse_chunk(ik_sse_parser_t *parser,
                         const char *chunk,
                         size_t len);
```

## Provider Interface Usage

### Non-Streaming Request

```c
// Caller (e.g., repl)
ik_provider_t *provider = agent->provider;  // Lazy-initialized

ik_request_t *req = ik_request_create(ctx);
ik_request_add_message(req, IK_ROLE_USER, "Hello!");
ik_request_set_thinking(req, IK_THINKING_MED);

ik_response_t *resp = NULL;
res_t result = provider->vt->send(provider->impl_ctx, req, &resp);
if (is_err(&result)) {
    // Handle error
}

// Process response
for (size_t i = 0; i < resp->content_count; i++) {
    if (resp->content[i].type == IK_CONTENT_TEXT) {
        display_text(resp->content[i].text);
    }
}
```

### Streaming Request

```c
// Stream callback receives normalized events
void on_stream_event(ik_stream_event_t *event, void *user_ctx) {
    switch (event->type) {
        case IK_STREAM_TEXT_DELTA:
            append_to_scrollback(event->data.text_delta.text);
            break;
        case IK_STREAM_THINKING_DELTA:
            append_thinking(event->data.thinking_delta.text);
            break;
        case IK_STREAM_DONE:
            finalize_message(event->data.done.usage);
            break;
    }
}

// Make streaming request
res_t result = provider->vt->stream(provider->impl_ctx,
                                    req,
                                    on_stream_event,
                                    user_ctx);
```

## Provider Cleanup

Cleanup happens when provider is freed (via talloc):

```c
void ik_anthropic_cleanup(void *impl_ctx)
{
    ik_anthropic_ctx_t *ctx = impl_ctx;

    // Cleanup any persistent connections, etc.
    // HTTP client cleaned up via talloc hierarchy

    (void)ctx;  // May not need explicit cleanup if using talloc properly
}
```

Talloc hierarchy ensures everything is cleaned up:

```
provider (talloc parent)
  └─> impl_ctx (provider-specific)
       └─> http_client
       └─> api_key (strdup)
       └─> other resources
```

## Provider Registry

Optional: Cache providers per agent to avoid recreating:

```c
// In ik_agent_ctx_t
typedef struct {
    // ...
    char *provider_name;           // "anthropic"
    char *model;                   // "claude-sonnet-4-5"
    ik_thinking_level_t thinking;  // IK_THINKING_MED

    ik_provider_t *provider;       // Cached, NULL until first use
} ik_agent_ctx_t;
```

## Integration Points

### REPL Changes

The REPL layer becomes provider-agnostic:

```c
// Before (rel-06): Hard-coded OpenAI
res_t result = ik_openai_stream(repl->llm, messages, ...);

// After (rel-07): Provider abstraction
ik_provider_t *provider = NULL;
TRY(ik_agent_get_provider(agent, &provider));
res_t result = provider->vt->stream(provider->impl_ctx, req, ...);
```

### Command Updates

`/model` command now supports provider inference:

```c
// Parse: /model claude-sonnet-4-5/med
// Infer: provider = "anthropic" (from "claude-" prefix)
// Set:   agent->provider = "anthropic"
//        agent->model = "claude-sonnet-4-5"
//        agent->thinking = IK_THINKING_MED
```

## Migration from Existing OpenAI Code

### Current Structure (rel-06)

```
src/openai/
  client.c              → REPL calls directly
  client_multi.c
  http_handler.c
  sse_parser.c
```

### New Structure (rel-07)

```
src/providers/
  common/
    http_client.c       ← Refactored from http_handler.c
    sse_parser.c        ← Moved from openai/sse_parser.c
  openai/
    adapter.c           ← NEW: vtable implementation
    client.c            ← Refactored from openai/client.c
    streaming.c         ← Refactored from openai/client_multi*.c
    openai.h
```

### Refactoring Strategy

1. **Extract shared HTTP logic** → `common/http_client.c`
2. **Move SSE parser** → `common/sse_parser.c` (rename from `ik_openai_*` to `ik_sse_*`)
3. **Create adapter** → `openai/adapter.c` implements vtable
4. **Refactor client** → OpenAI-specific request building in `openai/client.c`
5. **Remove old directory** → Delete `src/openai/` after migration

## Error Handling

Providers return errors through standard `res_t` mechanism:

```c
res_t ik_anthropic_send(...) {
    // HTTP request
    int status = 0;
    char *response = NULL;
    res_t http_result = ik_http_post(ctx, "/v1/messages", headers, body,
                                      &response, &status);
    if (is_err(&http_result)) {
        return http_result;  // Network error
    }

    // Check HTTP status
    if (status == 401) {
        return ERR(ctx, ERR_AUTH,
                   "Invalid Anthropic API key. Get key at: https://console.anthropic.com/settings/keys");
    }

    if (status == 429) {
        // Parse retry-after header
        return ERR(ctx, ERR_RATE_LIMIT,
                   "Rate limit exceeded. Retry after %d seconds", retry_after);
    }

    // Parse response...
}
```

See [error-handling.md](error-handling.md) for full error mapping strategy.

## Testing

Each provider has its own test suite:

```
tests/unit/
  providers/
    test_anthropic_adapter.c   # Vtable implementation
    test_anthropic_client.c    # Request building
    test_openai_adapter.c
    test_google_adapter.c

  common/
    test_http_client.c         # Shared utilities
    test_sse_parser.c
```

Mock HTTP responses using existing `MOCKABLE` pattern:

```c
START_TEST(test_anthropic_send_basic) {
    mock_http_response(200, ANTHROPIC_RESPONSE_FIXTURE);

    ik_provider_t *provider = NULL;
    TRY(ik_anthropic_create(ctx, "sk-ant-test", &provider));

    ik_request_t *req = create_test_request();
    ik_response_t *resp = NULL;

    res_t result = provider->vt->send(provider->impl_ctx, req, &resp);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(resp->content[0].text, "Hello!");
}
END_TEST
```

See [testing-strategy.md](testing-strategy.md) for full testing approach.
