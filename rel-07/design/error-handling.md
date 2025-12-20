# Error Handling Strategy

## Overview

Provider adapters map provider-specific HTTP errors to ikigai's unified error categories. Errors include both category (for programmatic handling) and provider details (for debugging).

## Error Structure

```c
typedef enum {
    ERR_AUTH,           // Invalid credentials
    ERR_RATE_LIMIT,     // Rate limit exceeded
    ERR_INVALID_ARG,    // Bad request / validation error
    ERR_NOT_FOUND,      // Model not found
    ERR_SERVER,         // Server error (500, 502, 503)
    ERR_TIMEOUT,        // Request timeout
    ERR_CONTENT_FILTER, // Content policy violation
    ERR_NETWORK,        // Network/connection error
    ERR_UNKNOWN         // Other/unmapped errors
} ik_error_category_t;

typedef struct {
    ik_error_category_t category;
    int http_status;             // HTTP status code (0 if not HTTP)
    char *message;               // Human-readable message
    char *provider_code;         // Provider's error type
    int retry_after_ms;          // Retry delay (-1 if not applicable)
} ik_error_t;
```

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

**Adapter implementation:**
```c
res_t ik_anthropic_handle_error(TALLOC_CTX *ctx, int status, const char *response)
{
    // Parse error JSON
    yyjson_doc *doc = yyjson_read(response, strlen(response), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    const char *type = yyjson_get_str(yyjson_obj_get(error_obj, "type"));
    const char *message = yyjson_get_str(yyjson_obj_get(error_obj, "message"));

    ik_error_category_t category = ERR_UNKNOWN;
    int retry_after_ms = -1;

    if (status == 401) {
        category = ERR_AUTH;
        message = talloc_asprintf(ctx,
            "Invalid Anthropic API key. Get key at: https://console.anthropic.com/settings/keys");
    }
    else if (status == 429) {
        category = ERR_RATE_LIMIT;
        retry_after_ms = parse_retry_after_header(response_headers) * 1000;
    }
    else if (status == 400) {
        category = ERR_INVALID_ARG;
    }
    else if (status >= 500) {
        category = ERR_SERVER;
    }

    yyjson_doc_free(doc);

    return ERR_DETAILED(ctx, category, status, type, retry_after_ms, "%s", message);
}
```

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

**Adapter implementation:**
```c
res_t ik_openai_handle_error(TALLOC_CTX *ctx, int status, const char *response)
{
    yyjson_doc *doc = yyjson_read(response, strlen(response), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    const char *code = yyjson_get_str(yyjson_obj_get(error_obj, "code"));
    const char *message = yyjson_get_str(yyjson_obj_get(error_obj, "message"));

    ik_error_category_t category = ERR_UNKNOWN;
    int retry_after_ms = -1;

    if (status == 401) {
        category = ERR_AUTH;
        message = talloc_asprintf(ctx,
            "Invalid OpenAI API key. Get key at: https://platform.openai.com/api-keys");
    }
    else if (status == 429) {
        category = ERR_RATE_LIMIT;
        // OpenAI uses x-ratelimit-reset-* headers
        retry_after_ms = parse_openai_reset_header(response_headers);
    }
    else if (status == 400) {
        category = ERR_INVALID_ARG;
    }
    else if (status == 404) {
        category = ERR_NOT_FOUND;
    }
    else if (status >= 500) {
        category = ERR_SERVER;
    }

    yyjson_doc_free(doc);

    return ERR_DETAILED(ctx, category, status, code, retry_after_ms, "%s", message);
}
```

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

**Adapter implementation:**
```c
res_t ik_google_handle_error(TALLOC_CTX *ctx, int status, const char *response)
{
    yyjson_doc *doc = yyjson_read(response, strlen(response), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    const char *code_str = yyjson_get_str(yyjson_obj_get(error_obj, "status"));
    const char *message = yyjson_get_str(yyjson_obj_get(error_obj, "message"));

    ik_error_category_t category = ERR_UNKNOWN;
    int retry_after_ms = -1;

    if (status == 403) {
        category = ERR_AUTH;
        message = talloc_asprintf(ctx,
            "Invalid Google API key. Get key at: https://aistudio.google.com");
    }
    else if (status == 429) {
        category = ERR_RATE_LIMIT;
        // Google includes retryDelay in error response
        retry_after_ms = yyjson_get_int(yyjson_obj_get(error_obj, "retryDelay")) * 1000;
    }
    else if (status == 400) {
        category = ERR_INVALID_ARG;
    }
    else if (status == 404) {
        category = ERR_NOT_FOUND;
    }
    else if (status == 504) {
        category = ERR_TIMEOUT;
    }
    else if (status >= 500) {
        category = ERR_SERVER;
    }

    yyjson_doc_free(doc);

    return ERR_DETAILED(ctx, category, status, code_str, retry_after_ms, "%s", message);
}
```

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

### Exponential Backoff

For retryable errors (server errors, rate limits):

```c
int retry_count = 0;
int max_retries = 3;
int base_delay_ms = 1000;

while (retry_count < max_retries) {
    res_t result = provider->vt->send(provider->impl_ctx, req, &resp);

    if (is_ok(&result)) {
        return result;  // Success
    }

    ik_error_t *error = &result.err;

    // Check if retryable
    bool retryable = (error->category == ERR_RATE_LIMIT ||
                     error->category == ERR_SERVER ||
                     error->category == ERR_TIMEOUT);

    if (!retryable) {
        return result;  // Don't retry
    }

    // Calculate delay
    int delay_ms;
    if (error->retry_after_ms > 0) {
        // Use provider's suggestion
        delay_ms = error->retry_after_ms;
    } else {
        // Exponential backoff with jitter
        int backoff = base_delay_ms * (1 << retry_count);  // 1s, 2s, 4s
        int jitter = rand() % 1000;  // 0-1000ms
        delay_ms = backoff + jitter;
    }

    // Sleep
    usleep(delay_ms * 1000);
    retry_count++;
}

return result;  // Max retries exceeded
```

### Non-Retryable Errors

These errors fail immediately:
- `ERR_AUTH` - Credentials are invalid, retry won't help
- `ERR_INVALID_ARG` - Request is malformed
- `ERR_NOT_FOUND` - Model doesn't exist
- `ERR_CONTENT_FILTER` - Content violates policy

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

Provider errors should be logged with full details:

```c
if (is_err(&result)) {
    ik_error_t *err = &result.err;

    ik_log(LOG_ERROR,
           "Provider request failed: provider=%s, category=%s, http_status=%d, "
           "provider_code=%s, message=%s, retry_after_ms=%d",
           provider->name,
           ik_error_category_name(err->category),
           err->http_status,
           err->provider_code ? err->provider_code : "N/A",
           err->message,
           err->retry_after_ms);
}
```

## Testing

### Mock Error Responses

```c
START_TEST(test_anthropic_auth_error) {
    const char *error_response =
        "{\"type\":\"error\",\"error\":{\"type\":\"authentication_error\","
        "\"message\":\"Invalid API key\"}}";

    mock_http_response(401, error_response);

    ik_provider_t *provider = create_anthropic_provider("bad-key");
    ik_request_t *req = create_test_request();
    ik_response_t *resp = NULL;

    res_t result = provider->vt->send(provider->impl_ctx, req, &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err.category, ERR_AUTH);
    ck_assert_int_eq(result.err.http_status, 401);
    ck_assert_str_contains(result.err.message, "API key");
}
END_TEST
```

### Retry Logic Testing

```c
START_TEST(test_retry_on_rate_limit) {
    // First call: 429 rate limit
    mock_http_response(429, "{\"error\":{\"type\":\"rate_limit_error\"}}");

    // Second call: success
    mock_http_response_sequence({
        {429, rate_limit_json},
        {200, success_json}
    });

    int retry_count = 0;
    auto count_retries = lambda(void, (void) { retry_count++; });
    mock_sleep_callback(count_retries);

    res_t result = send_with_retry(provider, req);

    ck_assert(is_ok(&result));
    ck_assert_int_eq(retry_count, 1);  // Retried once
}
END_TEST
```

## Error Category Utilities

```c
// Convert category enum to string
const char *ik_error_category_name(ik_error_category_t category);

// Check if error is retryable
bool ik_error_is_retryable(ik_error_t *error);

// Get user-facing error message
char *ik_error_user_message(TALLOC_CTX *ctx, ik_error_t *error, const char *provider);
```
