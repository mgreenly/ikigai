# Error Handling Strategy

## Overview

Provider adapters map provider-specific HTTP errors to ikigai's unified error categories. Errors include both category (for programmatic handling) and provider details (for debugging).

**Error delivery:** Errors are delivered via completion callbacks, not return values from start_request/start_stream. The async nature means errors may be detected during perform() or info_read().

## Error Categories

- **ERR_AUTH** - Invalid credentials
- **ERR_RATE_LIMIT** - Rate limit exceeded
- **ERR_INVALID_ARG** - Bad request / validation error
- **ERR_NOT_FOUND** - Model not found
- **ERR_SERVER** - Server error (500, 502, 503)
- **ERR_TIMEOUT** - Request timeout
- **ERR_CONTENT_FILTER** - Content policy violation
- **ERR_NETWORK** - Network/connection error
- **ERR_UNKNOWN** - Other/unmapped errors

## Error Structure Fields

**Naming:** Uses `ik_provider_error_t` (not `ik_error_t`) to avoid collision with existing `err_t` in src/error.h. The existing `err_t` is for general result types; `ik_provider_error_t` is specifically for provider API errors with HTTP status, retry info, etc.

| Field | Type | Description |
|-------|------|-------------|
| `category` | enum | Error category (see above) |
| `http_status` | int | HTTP status code (0 if not HTTP) |
| `message` | string | Human-readable message |
| `provider_code` | string | Provider's error type |
| `retry_after_ms` | int | Retry delay (-1 if not applicable) |

## Provider Error Mapping

### Anthropic

| HTTP Status | Provider Type | ikigai Category | Retry | Notes |
|-------------|---------------|-----------------|-------|-------|
| 401 | `authentication_error` | `ERR_AUTH` | No | Invalid API key |
| 403 | `permission_error` | `ERR_AUTH` | No | Insufficient permissions |
| 429 | `rate_limit_error` | `ERR_RATE_LIMIT` | Yes | Check `retry-after` header |
| 400 | `invalid_request_error` | `ERR_INVALID_ARG` | No | Bad request format |
| 404 | `not_found_error` | `ERR_NOT_FOUND` | No | Unknown model |
| 500 | `api_error` | `ERR_SERVER` | Yes | Internal server error |
| 529 | `overloaded_error` | `ERR_SERVER` | Yes | Server overloaded |

**Error response format:**
```json
{
  "type": "error",
  "error": {
    "type": "rate_limit_error",
    "message": "Your request was rate-limited"
  }
}
```

**Adapter responsibilities:**
- Parse error JSON response body
- Extract provider error type and message
- Map HTTP status to ikigai error category
- Extract retry-after header for rate limits (in seconds)
- Build user-friendly message with provider-specific help URLs
- For auth errors: Include credential configuration instructions
- For rate limits: Parse retry-after header and convert to milliseconds
- For server errors: Mark as retryable
- Return unified error structure with all fields populated

### OpenAI

| HTTP Status | Provider Code | ikigai Category | Retry | Notes |
|-------------|---------------|-----------------|-------|-------|
| 401 | `invalid_api_key` | `ERR_AUTH` | No | Invalid API key |
| 401 | `invalid_org` | `ERR_AUTH` | No | Invalid organization |
| 429 | `rate_limit_exceeded` | `ERR_RATE_LIMIT` | Yes | Check rate limit headers |
| 429 | `quota_exceeded` | `ERR_RATE_LIMIT` | No | Monthly quota exceeded |
| 400 | `invalid_request_error` | `ERR_INVALID_ARG` | No | Bad request |
| 404 | `model_not_found` | `ERR_NOT_FOUND` | No | Unknown model |
| 500 | `server_error` | `ERR_SERVER` | Yes | Server error |
| 503 | `service_unavailable` | `ERR_SERVER` | Yes | Server overloaded |

**Error response format:**
```json
{
  "error": {
    "message": "Incorrect API key provided",
    "type": "invalid_request_error",
    "code": "invalid_api_key"
  }
}
```

**Adapter responsibilities:**
- Parse error JSON response body
- Extract provider error code and message
- Map HTTP status to ikigai error category
- Parse OpenAI rate limit reset headers (format: "6m0s")
- Build user-friendly message with provider-specific help URLs
- For auth errors: Include credential configuration instructions
- For rate limits: Calculate retry delay from x-ratelimit-reset-* headers
- For server errors: Mark as retryable
- Return unified error structure with all fields populated

### Google

| HTTP Status | Provider Code | ikigai Category | Retry | Notes |
|-------------|---------------|-----------------|-------|-------|
| 403 | `PERMISSION_DENIED` | `ERR_AUTH` | No | Invalid or leaked API key |
| 429 | `RESOURCE_EXHAUSTED` | `ERR_RATE_LIMIT` | Yes | Check `retryDelay` in response |
| 400 | `INVALID_ARGUMENT` | `ERR_INVALID_ARG` | No | Bad request |
| 404 | `NOT_FOUND` | `ERR_NOT_FOUND` | No | Unknown model |
| 500 | `INTERNAL` | `ERR_SERVER` | Yes | Internal error |
| 503 | `UNAVAILABLE` | `ERR_SERVER` | Yes | Service unavailable |
| 504 | `DEADLINE_EXCEEDED` | `ERR_TIMEOUT` | Yes | Request timeout |

**Error response format:**
```json
{
  "error": {
    "code": 403,
    "message": "Your API key was reported as leaked...",
    "status": "PERMISSION_DENIED"
  }
}
```

**Adapter responsibilities:**
- Parse error JSON response body
- Extract provider status code and message
- Map HTTP status to ikigai error category
- Extract retryDelay from response body (format: "60s")
- Build user-friendly message with provider-specific help URLs
- For auth errors: Include credential configuration instructions
- For rate limits: Parse retryDelay from error body and convert to milliseconds
- For server errors: Mark as retryable
- For timeouts: Map DEADLINE_EXCEEDED to ERR_TIMEOUT
- Return unified error structure with all fields populated

## Content Filter Error Mapping

Each provider returns content policy violations differently. All must map to `IK_ERR_CAT_CONTENT_FILTER`:

| Provider | HTTP Status | Error Type/Field | Detection | Maps To |
|----------|-------------|------------------|-----------|---------|
| Anthropic | 400 | `error.type == "invalid_request_error"` with message containing "content" | Check error message for policy keywords | IK_ERR_CAT_CONTENT_FILTER |
| OpenAI | 400 | `error.code == "content_filter"` | Check error.code field | IK_ERR_CAT_CONTENT_FILTER |
| Google | 200 | `candidates[0].finishReason == "SAFETY"` | Check finishReason in response | IK_ERR_CAT_CONTENT_FILTER |
| Google | 200 | `promptFeedback.blockReason == "SAFETY"` | Check promptFeedback block | IK_ERR_CAT_CONTENT_FILTER |

**Important:** Google returns HTTP 200 for content filter blocks - check response body, not status code.

**User-facing message:** When content filter is triggered, display:
```
Content policy violation: Your request was blocked by the provider's safety filters.
```

## Rate Limit Headers

**Design Decision:** Rate limit header parsing is **decentralized per-provider**. No shared `ik_parse_rate_limit_headers()` function is created.

**Rationale:**
1. **Header names differ completely** - Each provider uses different naming conventions (anthropic-ratelimit-*, x-ratelimit-*, vs no headers)
2. **Data formats vary** - Anthropic uses integers, OpenAI uses duration strings ("6m0s"), Google uses JSON body with duration ("60s")
3. **Semantics differ** - Anthropic provides raw limits/remaining, OpenAI provides reset times, Google only provides retry delay in errors
4. **Google is special** - No HTTP headers at all; rate limit info only appears in error response body
5. **Per-provider logic is simpler** - Each adapter knows exactly which headers to look for and how to parse them
6. **No abstraction benefit** - A shared parser would need conditionals for each provider anyway

**Implementation approach:**
- Each provider adapter parses its own headers in the HTTP response handler
- Anthropic: Extract integer values from `anthropic-ratelimit-*` and `retry-after` headers
- OpenAI: Extract integers and parse duration strings from `x-ratelimit-*` headers
- Google: Parse `retryDelay` from error response JSON body (no headers)
- All providers populate the same `retry_after_ms` field in `ik_provider_error_t`

### Anthropic

```
anthropic-ratelimit-requests-limit: 1000
anthropic-ratelimit-requests-remaining: 999
anthropic-ratelimit-tokens-limit: 100000
anthropic-ratelimit-tokens-remaining: 99950
retry-after: 20  // On 429 only, in seconds
```

**Parsing logic:**
- Extract `retry-after` header value (integer seconds)
- Convert to milliseconds: `retry_after_ms = retry_after * 1000`
- Other headers are informational only (for logging/debugging)

### OpenAI

```
x-ratelimit-limit-requests: 5000
x-ratelimit-remaining-requests: 4999
x-ratelimit-reset-requests: 6m0s
x-ratelimit-limit-tokens: 800000
x-ratelimit-remaining-tokens: 799500
x-ratelimit-reset-tokens: 3m20s
```

**Parsing logic:**
- Parse duration string format: "Xm Ys" or "Xs" (e.g., "6m0s", "3m20s", "45s")
- Extract minutes and seconds, convert to total milliseconds
- Use whichever reset time is sooner (requests vs tokens)
- Example: "6m0s" → 360,000ms

### Google

No standard headers. Rate limit info in error response body:

```json
{
  "error": {
    "code": 429,
    "status": "RESOURCE_EXHAUSTED",
    "message": "Quota exceeded for requests per minute",
    "retryDelay": "60s"  // Suggested delay
  }
}
```

**Parsing logic:**
- Extract `retryDelay` string from error JSON (format: "Xs")
- Parse integer and convert to milliseconds
- Example: "60s" → 60,000ms
- Field may be absent; use exponential backoff if missing

## Retry Strategy

### Retryable Error Categories

The following error categories should be retried with exponential backoff:
- **ERR_RATE_LIMIT** - Rate limit exceeded, retry after delay
- **ERR_SERVER** - Server errors (500, 502, 503, 529)
- **ERR_TIMEOUT** - Request timeout

### Non-Retryable Error Categories

These errors fail immediately without retry:
- **ERR_AUTH** - Credentials are invalid, retry won't help
- **ERR_INVALID_ARG** - Request is malformed
- **ERR_NOT_FOUND** - Model doesn't exist
- **ERR_CONTENT_FILTER** - Content violates policy
- **ERR_NETWORK** - Network/connection error
- **ERR_UNKNOWN** - Unmapped errors

### Retry Flow (Async)

Retries are handled asynchronously to avoid blocking the event loop:

1. Start request via provider's `start_request()` or `start_stream()` (returns immediately)
2. Event loop processes I/O via `perform()` / `info_read()`
3. When transfer completes, completion callback receives result
4. If success: Callback processes response
5. If error: Callback checks error category for retryability
6. If non-retryable: Callback reports error to user
7. If retryable and retries remaining:
   - Calculate delay (provider suggestion or exponential backoff)
   - Schedule retry using timer in event loop (NOT blocking sleep)
   - When timer fires, call `start_request()` again
8. If max retries exceeded: Report last error to user

**Important:** Never use blocking `sleep()` or `usleep()` - this would freeze the REPL. Instead, the retry delay is implemented as a timeout in the select() call, allowing the UI to remain responsive.

### Retry Timer Integration with Event Loop

The provider's `timeout()` method integrates retries with the select()-based event loop:

1. Request fails -> provider records "retry needed at time X"
2. REPL calls `provider->timeout()` -> returns ms until retry (or -1 if none)
3. `select()` uses minimum timeout across all sources
4. `select()` wakes on timeout -> REPL calls `provider->perform()`
5. `perform()` checks if retry time reached -> initiates retry request
6. Cycle repeats until success or max retries exhausted

The provider tracks retry state internally. The REPL only needs to:
- Include provider FDs in `select()`
- Call `perform()` when `select()` returns
- Honor the `timeout()` value

### Backoff Calculation

For retryable errors, delays are calculated as follows:

1. **Maximum retries**: 3 attempts
2. **Base delay**: 1000ms
3. **Delay source**:
   - If error includes `retry_after_ms > 0`: Use provider's suggested delay
   - Otherwise: Use exponential backoff with jitter
     - Attempt 1: 1s + random(0-1s)
     - Attempt 2: 2s + random(0-1s)
     - Attempt 3: 4s + random(0-1s)
4. **Jitter**: Add 0-1000ms random delay to prevent thundering herd
5. **Failure**: If all retries exhausted, return last error

## User-Facing Error Messages

### Authentication Errors

```
❌ Authentication failed: anthropic

Invalid API key or missing credentials.

To fix:
  • Set ANTHROPIC_API_KEY environment variable, or
  • Add to ~/.config/ikigai/credentials.json:
    {
      "anthropic": {
        "api_key": "sk-ant-api03-..."
      }
    }

Get your API key at: https://console.anthropic.com/settings/keys
```

### Rate Limit Errors

```
⚠️  Rate limit exceeded: openai

You've exceeded your requests-per-minute quota.

Retrying automatically in 60 seconds...
(Attempt 1 of 3)
```

### Server Errors

```
⚠️  Server error: google

Google's API is temporarily unavailable (503).

Retrying automatically in 2 seconds...
(Attempt 1 of 3)
```

### Content Filter Errors

```
❌ Content filtered: anthropic

Your message was blocked by Anthropic's content policy.

This request cannot be retried. Please modify your message.
```

## Logging

Provider errors should be logged with full details for debugging:

**Log fields to include:**
- Provider name
- Error category (as string)
- HTTP status code
- Provider-specific error code
- Error message
- Retry delay (if applicable)

**Log level**: ERROR for all provider failures

**Format**: Structured key-value pairs for easy parsing and filtering

## Utility Functions

The error handling system provides these utility functions:

```c
/**
 * Get human-readable name for error category
 *
 * @param category Error category enum value
 * @return         String name of category (e.g., "ERR_AUTH", "ERR_RATE_LIMIT")
 *
 * Returns a static string suitable for logging and debugging.
 * Never returns NULL - unknown categories return "ERR_UNKNOWN".
 */
const char *ik_error_category_name(ik_error_category_t category);

/**
 * Check if error category is retryable
 *
 * @param category Error category enum value
 * @return         true if category should be retried, false otherwise
 *
 * Retryable categories:
 * - ERR_RATE_LIMIT (with delay)
 * - ERR_SERVER (with exponential backoff)
 * - ERR_TIMEOUT (with exponential backoff)
 *
 * Non-retryable categories:
 * - ERR_AUTH (credentials invalid)
 * - ERR_INVALID_ARG (request malformed)
 * - ERR_NOT_FOUND (model doesn't exist)
 * - ERR_CONTENT_FILTER (policy violation)
 * - ERR_NETWORK (connection failure)
 * - ERR_UNKNOWN (unmapped errors)
 */
bool ik_error_is_retryable(ik_error_category_t category);

/**
 * Build user-facing error message with help text
 *
 * @param ctx Talloc context for allocation
 * @param err Provider error structure with category, status, message, etc.
 * @return    Formatted error message with fix instructions (caller owns, talloc'd on ctx)
 *
 * Generates user-friendly error messages including:
 * - Error category and provider name
 * - Human-readable description
 * - Fix instructions (for auth errors: credential setup steps)
 * - Retry information (for rate limits: countdown and attempt number)
 * - Help URLs for credential acquisition
 *
 * Example output for auth error:
 *   "Authentication failed: anthropic
 *    Invalid API key or missing credentials.
 *    To fix: Set ANTHROPIC_API_KEY or add to credentials.json
 *    Get your API key at: https://console.anthropic.com/settings/keys"
 */
char *ik_error_user_message(TALLOC_CTX *ctx, const ik_provider_error_t *err);
```
