# Task: Implement Provider Error Handling

**Layer:** 1
**Model:** sonnet/thinking
**Depends on:** provider-types.md

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/error.c` - Error handling implementation
- `src/error.h` - Error types and functions

**Plan:**
- `scratch/plan/error-handling.md` - Full error handling specification

## Objective

Implement error mapping and retry logic for all providers. Create shared utilities for categorizing HTTP errors, extracting retry-after information, and generating user-facing error messages that help users resolve authentication, rate limiting, and other provider errors.

## Interface

### Shared Error Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_error_category_name` | `const char *(ik_error_category_t category)` | Convert category to string name |
| `ik_error_is_retryable` | `bool (ik_error_category_t category)` | Check if error can be retried |
| `ik_error_user_message` | `char *(TALLOC_CTX *ctx, const char *provider, ik_error_category_t category, const char *detail)` | Generate user-facing error message |

### Provider-Specific Error Handlers

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_anthropic_handle_error` | `res_t (TALLOC_CTX *ctx, int32_t status, const char *body, ik_error_category_t *out_category)` | Map Anthropic HTTP errors to categories |
| `ik_openai_handle_error` | `res_t (TALLOC_CTX *ctx, int32_t status, const char *body, ik_error_category_t *out_category)` | Map OpenAI HTTP errors to categories |
| `ik_google_handle_error` | `res_t (TALLOC_CTX *ctx, int32_t status, const char *body, ik_error_category_t *out_category)` | Map Google HTTP errors to categories |

### Retry-After Extraction

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_anthropic_get_retry_after` | `int32_t (const char **headers)` | Extract retry-after from Anthropic response headers |
| `ik_openai_get_retry_after` | `int32_t (const char **headers)` | Extract retry delay from OpenAI rate limit headers |
| `ik_google_get_retry_after` | `int32_t (const char *body)` | Extract retryDelay from Google response body JSON |

## Behaviors

### Error Category Mapping

Map HTTP status codes to `ik_error_category_t`:
- 401 → IK_ERR_CAT_AUTH
- 429 → IK_ERR_CAT_RATE_LIMIT
- 400 → IK_ERR_CAT_INVALID_ARG
- 404 → IK_ERR_CAT_NOT_FOUND
- 500, 502, 503 → IK_ERR_CAT_SERVER
- Timeout (no response) → IK_ERR_CAT_TIMEOUT
- Content filter (provider-specific) → IK_ERR_CAT_CONTENT_FILTER
- Network errors → IK_ERR_CAT_NETWORK
- Other codes → IK_ERR_CAT_UNKNOWN

### Retryable Categories

Categories that can be retried:
- RATE_LIMIT (with delay)
- SERVER (with backoff)
- TIMEOUT (immediate retry)
- NETWORK (with backoff)

Non-retryable categories:
- AUTH (needs credential fix)
- INVALID_ARG (needs request fix)
- NOT_FOUND (needs model name fix)
- CONTENT_FILTER (content blocked)

### Category to String

Return human-readable names:
- AUTH → "authentication"
- RATE_LIMIT → "rate_limit"
- INVALID_ARG → "invalid_argument"
- NOT_FOUND → "not_found"
- SERVER → "server_error"
- TIMEOUT → "timeout"
- CONTENT_FILTER → "content_filter"
- NETWORK → "network_error"
- UNKNOWN → "unknown"

### User-Facing Messages

Generate helpful messages based on category:

- **AUTH**: "Authentication failed for {provider}. Check your API key in {env_var} or ~/.config/ikigai/credentials.json"
- **RATE_LIMIT**: "Rate limit exceeded for {provider}. Retry after {delay} seconds"
- **INVALID_ARG**: "Invalid request to {provider}: {detail}"
- **NOT_FOUND**: "Model not found on {provider}: {detail}"
- **SERVER**: "{provider} server error. This is temporary, retrying may succeed"
- **TIMEOUT**: "Request to {provider} timed out. Check network connection"
- **CONTENT_FILTER**: "Content blocked by {provider} safety filters: {detail}"
- **NETWORK**: "Network error connecting to {provider}: {detail}"
- **UNKNOWN**: "{provider} error: {detail}"

### Retry-After Extraction

**Anthropic**: Parse `retry-after` header (integer seconds)

**OpenAI**: Parse `x-ratelimit-reset-requests` or `x-ratelimit-reset-tokens` headers (Unix timestamp), calculate seconds until reset

**Google**: Parse response body JSON for `retryDelay` field (duration string like "30s"), convert to seconds

Return retry delay in seconds, or -1 if not available.

### Error Handling Flow

1. Provider makes HTTP request
2. If non-2xx status, call provider-specific error handler
3. Error handler parses response body/headers
4. Map to category, extract details
5. Return ERR with category and message
6. Caller checks retryability
7. If retryable, extract retry-after and schedule retry
8. Generate user message for display

### Memory Management

Error messages allocated via talloc on provided context. JSON parsing uses yyjson for error response bodies.

## Directory Structure

```
src/providers/common/
├── error.h
└── error.c

src/providers/anthropic/
└── error.c

src/providers/openai/
└── error.c

src/providers/google/
└── error.c

tests/unit/providers/common/
└── error_test.c

tests/unit/providers/anthropic/
└── error_test.c

tests/unit/providers/openai/
└── error_test.c

tests/unit/providers/google/
└── error_test.c
```

## Test Scenarios

### Common Error Tests (`common/error_test.c`)

- Category to string: All categories map to names
- Retryability: Retryable categories return true, others false
- User messages: Generated for each category with provider context
- Message formatting: Includes relevant details and suggestions

### Anthropic Error Tests

- 401 status: Maps to AUTH category
- 429 status: Maps to RATE_LIMIT, extracts retry-after
- 500 status: Maps to SERVER
- Unknown status: Maps to UNKNOWN
- Error body parsing: Extract error details from JSON

### OpenAI Error Tests

- 401 status: Maps to AUTH
- 429 status: Maps to RATE_LIMIT, calculates delay from reset timestamp
- Content filter: Detect and map to CONTENT_FILTER
- Error body parsing: Extract error message and type

### Google Error Tests

- 401 status: Maps to AUTH
- 429 status: Maps to RATE_LIMIT, extract retryDelay from body
- 404 status: Maps to NOT_FOUND
- Error body parsing: Extract error details from JSON

## Postconditions

- [ ] `src/providers/common/error.h` and `error.c` exist
- [ ] Provider-specific error handlers implemented
- [ ] All HTTP status codes mapped correctly
- [ ] Retry-after extraction works for each provider
- [ ] User messages are helpful and actionable
- [ ] Retryable categories identified correctly
- [ ] Makefile updated with new sources/headers
- [ ] Compiles without warnings
- [ ] All unit tests pass
- [ ] `make check` passes
