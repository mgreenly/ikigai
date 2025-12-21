# Task: OpenAI Provider Core Structure

**Layer:** 4
**Model:** sonnet/thinking
**Depends on:** openai-adapter-shim.md, provider-types.md, credentials-core.md, error-core.md

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/provider.h` - Vtable and type definitions
- `src/providers/common/error.h` - Shared error utilities
- `src/providers/openai/adapter_shim.c` - Existing shim to replace
- `src/credentials.h` - Credentials API

**Plan:**
- `scratch/plan/thinking-abstraction.md` - Reasoning effort mapping

## Objective

Create native OpenAI provider to replace the adapter shim. This establishes directory structure, headers, factory registration, and reasoning effort mapping. The shim remains functional until all 4 tasks complete; `cleanup-old-openai.md` deletes it.

## Key Differences from Anthropic/Google

| Aspect | Anthropic | Google | OpenAI |
|--------|-----------|--------|--------|
| Thinking | Token budget | Budget (2.5) or Level (3.x) | Effort string (low/med/high) |
| APIs | Single | Single | Two (Chat Completions + Responses) |
| System prompt | `system` field | `systemInstruction` | First message with role "system" |
| Tool args | Parsed object | Parsed object | **JSON string** (must parse) |

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_create(ctx, api_key, out_provider)` | Create OpenAI provider with Chat Completions API (default) |
| `ik_openai_create_with_options(ctx, api_key, use_responses_api, out_provider)` | Create OpenAI provider with optional Responses API mode |
| `ik_openai_is_reasoning_model(model)` | Check if model supports reasoning.effort parameter (o1, o3, o1-mini, o3-mini) |
| `ik_openai_reasoning_effort(level)` | Map internal thinking level to "low", "medium", "high", or NULL |
| `ik_openai_supports_temperature(model)` | Check if model supports temperature (reasoning models do NOT) |
| `ik_openai_prefer_responses_api(model)` | Determine if model should use Responses API (reasoning models perform 3% better) |
| `ik_openai_handle_error(ctx, status, body, out_category)` | Parse error response, map to category, extract details |
| `ik_openai_get_retry_after(headers)` | Extract retry delay from x-ratelimit-reset-* headers, returns seconds or -1 |

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_ctx_t` | ctx (TALLOC_CTX*), api_key (char*), base_url (char*), use_responses_api (bool) | Provider implementation context |

### Constants

- `IK_OPENAI_BASE_URL` = "https://api.openai.com"
- `IK_OPENAI_CHAT_ENDPOINT` = "/v1/chat/completions"
- `IK_OPENAI_RESPONSES_ENDPOINT` = "/v1/responses"
- Reasoning model prefixes: "o3", "o1", "o4" (array with NULL terminator)

## Behaviors

### Model Detection

- `ik_openai_is_reasoning_model()` checks if model name starts with recognized prefixes (o1, o3, o4)
- Must validate character after prefix is '\0', '-', or '_' to avoid false matches (e.g., "o30" is not reasoning)
- Return false for NULL or empty model names

### Reasoning Effort Mapping

- `IK_THINKING_NONE` → NULL (don't include reasoning config)
- `IK_THINKING_LOW` → "low"
- `IK_THINKING_MED` → "medium"
- `IK_THINKING_HIGH` → "high"
- Unknown levels → NULL

### API Selection

- Reasoning models (o1/o3) should prefer Responses API for 3% better performance
- Non-reasoning models (gpt-4o, gpt-5) use Chat Completions API
- `use_responses_api` context flag can override default behavior

### Temperature Support

- Reasoning models do NOT support temperature parameter
- Check via `!ik_openai_is_reasoning_model(model)`

### Provider Factory

- Factory functions allocate provider on talloc context
- Copy api_key and base_url to provider-owned memory
- Set vtable to `&OPENAI_VTABLE` (defined in openai.c)
- Provider name should be "openai"
- Vtable functions `send` and `stream` are forward-declared (implemented in later tasks)

### Error Handling

**Error Response Format:**
```json
{
  "error": {
    "message": "Incorrect API key provided",
    "type": "invalid_request_error",
    "code": "invalid_api_key"
  }
}
```

**HTTP Status to Category Mapping:**

| HTTP Status | Provider Code | Category |
|-------------|---------------|----------|
| 401 | `invalid_api_key` | `IK_ERR_CAT_AUTH` |
| 401 | `invalid_org` | `IK_ERR_CAT_AUTH` |
| 429 | `rate_limit_exceeded` | `IK_ERR_CAT_RATE_LIMIT` |
| 429 | `quota_exceeded` | `IK_ERR_CAT_RATE_LIMIT` |
| 400 | `invalid_request_error` | `IK_ERR_CAT_INVALID_ARG` |
| 404 | `model_not_found` | `IK_ERR_CAT_NOT_FOUND` |
| 500 | `server_error` | `IK_ERR_CAT_SERVER` |
| 503 | `service_unavailable` | `IK_ERR_CAT_SERVER` |

**Content Filter Detection:**
- Check for `content_filter` in error.code or error.type
- Map to `IK_ERR_CAT_CONTENT_FILTER`

**`ik_openai_get_retry_after()` Behavior:**
- Scan headers for `x-ratelimit-reset-requests` or `x-ratelimit-reset-tokens`
- Parse duration format: "6m0s" → 360 seconds, "30s" → 30 seconds
- Return minimum of both reset times (prefer requests over tokens)
- Return -1 if neither header present

### Directory Structure

```
src/providers/openai/
├── openai.h         - Public interface
├── openai.c         - Factory and vtable
├── reasoning.h      - Reasoning effort mapping
├── reasoning.c      - Reasoning implementation
├── error.h          - Error handling API
├── error.c          - Error handling implementation
├── adapter_shim.c   - (KEEP for now, deleted by cleanup task)
```

## Test Scenarios

### Reasoning Model Detection

- Verify o1, o1-mini, o1-preview identified as reasoning models
- Verify o3, o3-mini, o4-preview identified as reasoning models
- Verify gpt-4o, gpt-4o-mini, gpt-5 are NOT reasoning models
- Verify NULL and empty strings return false
- Verify "o30" is NOT identified as reasoning model (false prefix match)

### Reasoning Effort Mapping

- NONE → NULL
- LOW → "low"
- MED → "medium"
- HIGH → "high"

### Temperature Support

- Reasoning models (o1, o3) → false
- Regular models (gpt-4o, gpt-5) → true

### API Preference

- Reasoning models → prefer Responses API (true)
- Regular models → prefer Chat Completions (false)

### Provider Creation

- `ik_openai_create()` returns valid provider with default (Chat Completions)
- `ik_openai_create_with_options()` respects use_responses_api flag
- Provider name is "openai"
- Vtable has non-null send and stream function pointers
- API key and base URL copied to provider context

### Factory Integration

- `ik_provider_create(ctx, "openai", api_key, &provider)` uses native implementation
- Shim remains in codebase but is no longer referenced

### Error Handling Tests

- 401 status with `invalid_api_key` maps to IK_ERR_CAT_AUTH
- 429 status maps to IK_ERR_CAT_RATE_LIMIT
- 500 status maps to IK_ERR_CAT_SERVER
- Content filter code maps to IK_ERR_CAT_CONTENT_FILTER
- Parse error body: extract error.type, error.code, error.message
- Invalid JSON body returns ERR

### Retry-After Tests

- Header "x-ratelimit-reset-requests: 6m0s" returns 360
- Header "x-ratelimit-reset-tokens: 30s" returns 30
- Both headers present returns minimum value
- Missing headers returns -1
- Malformed duration returns -1

## Postconditions

- [ ] `src/providers/openai/openai.h` exists with factory declarations
- [ ] `src/providers/openai/openai.c` implements factory functions
- [ ] `src/providers/openai/reasoning.h` and `.c` exist
- [ ] `src/providers/openai/error.h` declares `ik_openai_handle_error()` and `ik_openai_get_retry_after()`
- [ ] `src/providers/openai/error.c` implements error handling with correct status mappings
- [ ] Reasoning model detection works for o1/o3 prefixes
- [ ] Reasoning effort mapping returns correct strings
- [ ] Temperature support check works correctly
- [ ] API preference logic implemented
- [ ] Provider factory updated in `provider_common.c`
- [ ] `adapter_shim.c` still exists but is unused
- [ ] Makefile updated with new sources
- [ ] All reasoning tests pass
- [ ] All error handling tests pass
- [ ] Compiles without warnings
- [ ] `make check` passes
