# Task: Create OpenAI Adapter Shim

**Layer:** 2
**Model:** sonnet/extended (complex refactoring)
**Depends on:** provider-types.md, http-client.md, sse-parser.md, credentials-core.md

## Pre-Read

**Skills:**
- `/load source-code` - Map of existing OpenAI implementation
- `/load memory` - talloc ownership patterns
- `/load errors` - Result type patterns

**Source:**
- `src/openai/client.c` - Existing OpenAI client interface
- `src/openai/client_multi.c` - Multi-request handling
- `src/openai/client_multi_callbacks.c` - Streaming callbacks
- `src/openai/client_multi_request.c` - Request construction
- `src/openai/client_msg.c` - Message serialization
- `src/openai/client_serialize.c` - JSON serialization
- `src/openai/http_handler.c` - HTTP request handling
- `src/openai/sse_parser.c` - Server-sent events parsing
- `src/openai/tool_choice.c` - Tool choice handling
- `src/credentials.h` - Credentials API

**Plan:**
- `scratch/plan/architecture.md` - Migration Strategy section

## Objective

Create a thin adapter layer that wraps existing `src/openai/` code behind the new vtable interface. This allows incremental migration without breaking existing functionality while establishing the provider abstraction pattern. The shim uses the credentials API to obtain the OpenAI API key and transforms between the new normalized format and the existing OpenAI-specific format.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_shim_send(void *impl_ctx, const ik_request_t *req, ik_response_t **resp)` | Synchronous request via existing client, transforms request/response |
| `res_t ik_openai_shim_stream(void *impl_ctx, const ik_request_t *req, ik_stream_callback_t callback, void *user_ctx)` | Streaming request via existing client, normalizes stream events |
| `res_t ik_openai_create(TALLOC_CTX *ctx, ik_credentials_t *creds, ik_provider_t **out)` | Factory function creating provider with shim vtable |
| `void ik_openai_shim_destroy(void *impl_ctx)` | Cleanup shim context, NULL-safe |

Helper functions:

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_transform_request(const ik_request_t *req, /* OpenAI format */ *out)` | Convert normalized request to existing OpenAI format |
| `res_t ik_openai_transform_response(/* OpenAI format */ *in, ik_response_t **out)` | Convert existing OpenAI response to normalized format |
| `res_t ik_openai_normalize_stream_event(/* OpenAI event */ *in, ik_stream_event_t **out)` | Convert OpenAI SSE event to normalized stream event |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_shim_ctx_t` | api_key, http_client, existing_client_state | Holds OpenAI-specific context and existing client handles |

Files to create:

- `src/providers/openai/adapter_shim.c` - Shim implementation
- `src/providers/openai/openai.h` - Public factory function

Files to update:

- `src/providers/provider_common.c` - Add OpenAI case to `ik_provider_create()`

## Behaviors

### Request Transformation
- Convert `ik_request_t` (normalized) to existing OpenAI client format
- Map `thinking_level` to appropriate OpenAI model parameter
- Preserve all message content, tool definitions, and parameters
- Return ERR_INVALID_ARG if request format is incompatible

### Response Transformation
- Convert OpenAI response format to `ik_response_t` (normalized)
- Extract message content, tool calls, and usage statistics
- Map OpenAI-specific fields to normalized equivalents
- Preserve provider-specific data in `provider_data` field

### Streaming
- Wrap existing OpenAI streaming callbacks
- Convert each SSE chunk to normalized `ik_stream_event_t`
- Accumulate partial deltas and emit complete events
- Handle errors mid-stream by returning ERR and stopping
- Ensure callback is invoked for all event types (content, tool calls, thinking, done, error)

### Credentials
- Obtain API key via `ik_credentials_get(creds, "openai")`
- Return ERR_MISSING_CREDENTIALS if key not found
- Pass API key to existing OpenAI client functions

### Memory Management
- Shim context allocated on provider's talloc context
- All transformations allocate on appropriate parent context
- Existing OpenAI code manages its own memory (unchanged)

### Error Handling
- Return ERR_MISSING_CREDENTIALS if API key not found
- Return ERR_INVALID_ARG if request transformation fails
- Propagate errors from existing OpenAI client as-is
- Wrap OpenAI errors in normalized error format where needed

## Test Scenarios

### Unit Tests
- Create OpenAI provider via factory function
- Transform request: normalized to OpenAI format
- Transform response: OpenAI format to normalized
- Normalize stream event: SSE chunk to normalized event
- Handle missing API key: returns ERR_MISSING_CREDENTIALS
- Handle invalid request format: returns ERR_INVALID_ARG

### Integration Tests
- Send synchronous request through shim to OpenAI API
- Stream request through shim with callback accumulation
- Verify all existing OpenAI tests pass through shim
- Create provider via `ik_provider_create("openai", &p)`
- Verify credentials loaded correctly
- Handle mid-stream errors gracefully

### Compatibility Tests
- All existing tests in `tests/unit/openai/` pass unchanged
- All existing tests in `tests/integration/openai/` pass unchanged
- No modifications to `src/openai/` files required

## Postconditions

- [ ] `src/providers/openai/adapter_shim.c` exists and compiles
- [ ] `src/providers/openai/openai.h` exists with factory function
- [ ] `ik_provider_create("openai", &p)` works
- [ ] All existing OpenAI tests pass
- [ ] `make check` passes
- [ ] No changes to `src/openai/` files
- [ ] Credentials loaded via `ik_credentials_get(creds, "openai")`
