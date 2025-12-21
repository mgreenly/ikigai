# Task: OpenAI Shim Send Implementation

**Layer:** 2
**Model:** sonnet/thinking
**Depends on:** openai-shim-request.md, openai-shim-response.md

## Pre-Read

**Skills:**
- `/load memory` - talloc ownership patterns
- `/load errors` - Result types
- `/load source-code` - OpenAI client structure

**Source:**
- `src/providers/openai/shim.h` - Shim context and transforms
- `src/openai/client.h` - ik_openai_chat_create() signature
- `src/openai/client.c` - ik_openai_chat_create() implementation
- `src/config.h` - ik_cfg_t structure (needed by existing client)

**Plan:**
- `scratch/plan/architecture.md` - Migration Strategy

## Objective

Implement the synchronous `send()` vtable function for the OpenAI shim. This wires together the request/response transforms with the existing `ik_openai_chat_create()` function, completing the non-streaming path through the provider abstraction.

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_shim_send(void *impl_ctx, const ik_request_t *req, ik_response_t **out)` | Vtable send implementation |

### Internal Helpers

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_shim_build_cfg(TALLOC_CTX *ctx, ik_openai_shim_ctx_t *shim, const ik_request_t *req, ik_cfg_t **out)` | Build temporary config for existing client |

### Files to Update

- `src/providers/openai/shim.c` - Replace stub send() with real implementation
- `src/providers/openai/shim.h` - Add helper declarations if needed

## Behaviors

### Send Flow

1. Cast `impl_ctx` to `ik_openai_shim_ctx_t*`
2. Transform request: `ik_openai_shim_transform_request()`
3. Build temporary `ik_cfg_t` with api_key and model from request
4. Call `ik_openai_chat_create()` with NULL stream callback (synchronous mode)
5. Transform response: `ik_openai_shim_transform_response()`
6. Return normalized response

### Config Building

The existing client expects `ik_cfg_t` with:
- `openai_api_key` - From shim context
- `openai_model` - From request->model
- `openai_temperature` - Default 0.7 (or from request if available)
- `openai_max_completion_tokens` - From request->max_output_tokens

Create minimal config struct on temporary context.

### Error Propagation

- Transform errors: propagate as-is
- HTTP errors from existing client: propagate as-is
- Wrap provider-specific errors in ERR_PROVIDER if needed

### Memory Management

- Temporary context for intermediate allocations
- Response allocated on caller's context
- Free temporary context before returning (success or error)
- On error, ensure no memory leaks

### Vtable Registration

Update factory function to register real send() instead of stub:
```c
vtable->send = ik_openai_shim_send;
vtable->stream = ik_openai_shim_stream_stub;  // Still stub
vtable->cleanup = ik_openai_shim_destroy;
```

## Test Scenarios

### Unit Tests (Mocked HTTP)
- Send simple text request: returns text response
- Send request triggering tool call: returns tool_call response
- Missing API key: returns ERR_MISSING_CREDENTIALS
- Empty messages: returns ERR_INVALID_ARG
- HTTP error: propagates error correctly

### Integration Tests (With Mock Server)
- Full request/response cycle through vtable
- Verify response content matches expected
- Verify tool calls parsed correctly

## Postconditions

- [ ] `send()` vtable function works end-to-end
- [ ] Request transform + existing client + response transform integrated
- [ ] Errors propagate correctly
- [ ] No memory leaks on success or error paths
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] No changes to `src/openai/` files
- [ ] Existing OpenAI tests still pass
