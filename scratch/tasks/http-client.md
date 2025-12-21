# Task: Create Shared HTTP Client

**Layer:** 1
**Model:** sonnet/thinking
**Depends on:** provider-types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load memory` - Talloc patterns
- `/load errors` - Result types

**Source:**
- `src/openai/http_handler.c` - Reference for curl patterns
- `src/openai/http_handler.h` - Reference for callback types

## Objective

Create `src/providers/common/http_client.h` and `http_client.c` - a shared HTTP client wrapper around libcurl that all providers will use for making POST requests with both non-streaming and SSE streaming support.

## Interface

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_http_client_t` | base_url, curl handle | HTTP client for making API requests |
| `ik_http_response_t` | body (string), status (int32) | HTTP response from non-streaming request |

### Callback Types

| Type | Signature | Purpose |
|------|-----------|---------|
| `ik_http_stream_cb_t` | `bool (*)(const char *data, size_t len, void *ctx)` | Callback for SSE streaming data, returns true to continue |

### Functions to Implement

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_http_client_create` | `res_t (TALLOC_CTX *ctx, const char *base_url, ik_http_client_t **out)` | Create HTTP client for a base URL |
| `ik_http_post` | `res_t (ik_http_client_t *client, TALLOC_CTX *ctx, const char *path, const char **headers, const char *body, ik_http_response_t **out)` | POST request (non-streaming) |
| `ik_http_post_stream` | `res_t (ik_http_client_t *client, const char *path, const char **headers, const char *body, ik_http_stream_cb_t cb, void *cb_ctx, int32_t *out_status)` | POST request (streaming SSE) |

## Behaviors

### Client Creation

- Create reusable CURL handle stored in client struct
- Store base_url for all requests
- Set talloc destructor to cleanup CURL handle
- Return OK with client, or ERR on curl initialization failure

### Non-Streaming POST

- Combine base_url + path for full URL
- Set Content-Type: application/json automatically
- Append additional headers from NULL-terminated array
- Accumulate response body in growing talloc buffer
- Return HTTP status code and body in response struct
- Return ERR with ERR_IO on network/curl errors

### Streaming POST

- Similar setup to non-streaming
- Invoke callback with each chunk of data as received
- Callback returns false to abort transfer
- Does not parse SSE format (raw data chunks)
- Return HTTP status code via out parameter
- Return ERR if transfer fails (unless user aborted)

### Memory Management

- Client and responses allocated via talloc
- Response body owned by response struct
- Temporary allocations freed after request
- CURL handle cleaned up via talloc destructor

### Error Handling

- CURL errors mapped to ERR_IO
- Include curl error message in error
- Non-200 status codes returned (not errors)

## Directory Structure

```
src/providers/common/
├── http_client.h
└── http_client.c

tests/unit/providers/common/
└── http_client_test.c
```

## Test Scenarios

Create `tests/unit/providers/common/http_client_test.c`:

- Client creation: Successfully create client with base URL
- Client cleanup: Talloc destructor cleans up CURL handle without crash
- Non-streaming POST: (Integration test - requires actual server)
- Streaming POST: (Integration test - requires actual server)
- Memory lifecycle: Parent talloc context frees all allocations

## Postconditions

- [ ] `src/providers/common/http_client.h` exists with API
- [ ] `src/providers/common/http_client.c` implements HTTP client
- [ ] Directory `src/providers/common/` created
- [ ] Makefile updated with new source/header
- [ ] CURL handle reused across requests
- [ ] Talloc destructor handles cleanup
- [ ] Compiles without warnings
- [ ] Unit tests pass
- [ ] `make check` passes
