# Error Handling Strategy

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations
MUST be non-blocking:

- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

Reference: `src/openai/client_multi.c`

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

## Rate Limit Headers

### Anthropic

```
anthropic-ratelimit-requests-limit: 1000
anthropic-ratelimit-requests-remaining: 999
anthropic-ratelimit-tokens-limit: 100000
anthropic-ratelimit-tokens-remaining: 99950
retry-after: 20  // On 429 only
```

### OpenAI

```
x-ratelimit-limit-requests: 5000
x-ratelimit-remaining-requests: 4999
x-ratelimit-reset-requests: 6m0s
x-ratelimit-limit-tokens: 800000
x-ratelimit-remaining-tokens: 799500
x-ratelimit-reset-tokens: 3m20s
```

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

- **ik_error_category_name()** - Convert category enum to string for logging
- **ik_error_is_retryable()** - Check if error category should be retried
- **ik_error_user_message()** - Generate user-facing error message with provider context
