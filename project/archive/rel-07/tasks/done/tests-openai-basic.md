# Task: Create OpenAI Provider Basic Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** openai-core.md, openai-request-chat.md, openai-response-chat.md, vcr-core.md, vcr-mock-integration.md, vcr-fixtures-setup.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking:
- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

Tests MUST exercise the async fdset/perform/info_read pattern, not blocking patterns.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/openai/` - OpenAI provider implementation
- `tests/helpers/mock_http.h` - Mock infrastructure (curl_multi mocks)
- `src/openai/client_multi.c` - Reference async implementation

**Plan:**
- `scratch/plan/testing-strategy.md` - Mock HTTP pattern, async test flow
- `scratch/plan/provider-interface.md` - Vtable specification with async methods

## Objective

Create tests for OpenAI provider adapter, request serialization, response parsing, and error handling. Covers both standard GPT models and o-series reasoning models. All tests use the async curl_multi mock pattern.

## Recording Fixtures

Before tests can run in playback mode, fixtures must be recorded from real API responses:

1. **Ensure valid credentials** - `OPENAI_API_KEY` environment variable must be set
2. **Run in record mode**:
   ```bash
   VCR_RECORD=1 make build/tests/unit/providers/openai/test_openai_adapter
   VCR_RECORD=1 ./build/tests/unit/providers/openai/test_openai_adapter
   ```
3. **Verify fixtures created** - Check `tests/fixtures/vcr/openai/*.jsonl` files exist
4. **Verify no credentials leaked** - `grep -r "sk-" tests/fixtures/vcr/openai/` returns nothing
5. **Commit fixtures** - Fixtures are committed to git for deterministic CI runs

**Note:** Fixtures only need re-recording when API behavior changes. Normal test runs use playback mode (VCR_RECORD unset).

## Interface

**Test files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/openai/test_openai_adapter.c` | Provider vtable implementation |
| `tests/unit/providers/openai/test_openai_client.c` | Request serialization to Chat Completions API |
| `tests/unit/providers/openai/test_openai_errors.c` | Error response parsing and mapping |

**Fixture files (VCR JSONL format):**

| File | Purpose |
|------|---------|
| `tests/fixtures/vcr/openai/response_basic.jsonl` | Standard chat completion response |
| `tests/fixtures/vcr/openai/response_tool_call.jsonl` | Response with tool_calls array |
| `tests/fixtures/vcr/openai/response_reasoning.jsonl` | Response from o-series model |
| `tests/fixtures/vcr/openai/error_auth.jsonl` | 401 authentication error |
| `tests/fixtures/vcr/openai/error_rate_limit.jsonl` | 429 rate limit error (synthetic) |

## Test Scenarios

**Adapter Tests (5 tests):**
- Create adapter with valid credentials
- Destroy adapter cleans up resources
- Vtable async methods (fdset, perform, timeout, info_read) are non-NULL
- start_request returns OK immediately (non-blocking)
- start_stream returns OK immediately (non-blocking)

**Async Request Tests (4 tests):**

These tests use the async fdset/perform/info_read pattern. Completion is delivered via callback.

```c
// Pattern: start_request + event loop until callback fires
static ik_response_t *captured_response;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx) {
    captured_response = completion->response;
    return OK(NULL);
}

START_TEST(test_non_streaming_request)
{
    captured_response = NULL;
    mock_set_response(200, load_fixture("openai/response_basic.jsonl"));

    res_t r = provider->vt->start_request(provider->ctx, req, test_completion_cb, NULL);
    ck_assert(is_ok(&r));  // Returns immediately

    // Drive event loop until complete
    while (captured_response == NULL) {
        fd_set rfds, wfds, efds;
        int max_fd = 0;
        provider->vt->fdset(provider->ctx, &rfds, &wfds, &efds, &max_fd);
        // select() call here in real code
        provider->vt->perform(provider->ctx, NULL);
        provider->vt->info_read(provider->ctx, NULL);
    }

    ck_assert_str_eq(captured_response->content, "expected");
}
END_TEST
```

- start_request with mock response triggers callback on info_read
- start_request with mock HTTP error triggers callback with error
- Callback receives response via ik_provider_completion_t
- Event loop drives fdset/perform/info_read cycle

**Request Serialization Tests (7 tests):**
- Build request with system and user messages
- Build request for o1 model with reasoning_effort
- Build request for gpt-5 model without reasoning_effort
- Build request with tool definitions
- Build request without optional fields
- Verify correct headers (API key, content-type)
- Verify JSON structure matches Chat Completions API

**Response Parsing Tests (6 tests):**
- Parse response with message content
- Parse response with tool_calls array
- Parse response from reasoning model
- Extract tool call ID and arguments
- Parse tool arguments from JSON string
- Extract usage metadata (tokens)

**Error Handling Tests (5 tests):**
- Parse authentication error (401)
- Parse rate limit error (429)
- Parse context length error (400)
- Parse model not found error (404)
- Map errors to correct categories

## HTTP Status Code Error Matrix

This matrix defines how each HTTP status code should be handled, mapped to internal errors, and presented to users. All errors are delivered asynchronously via completion callbacks.

### 401 Unauthorized

**Scenario:** Invalid or missing API key

**Expected Behavior:**
- Fail immediately (do not retry)
- Error delivered via completion callback with `success = false`

**Error Mapping:**
- Internal code: `IK_ERR_CAT_AUTH`
- Category: Authentication failure

**User-Facing Message Format:**
```
Authentication failed: Invalid or missing API key.
Please verify your OPENAI_API_KEY is correct.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/openai/error_auth.jsonl`
- Response body must include OpenAI error JSON:
  ```json
  {
    "error": {
      "message": "Incorrect API key provided: sk-****. You can find your API key at https://platform.openai.com/account/api-keys.",
      "type": "invalid_request_error",
      "param": null,
      "code": "invalid_api_key"
    }
  }
  ```
- Fixture must redact actual API key from both request headers and response body

**Test Coverage:**
- Parse error.type and error.message from response body
- Map to IK_ERR_CAT_AUTH
- Verify callback receives error (not thrown exception)
- Handle error.code = "invalid_api_key"

### 403 Forbidden

**Scenario:** Permission denied, organization-level restrictions, or geographic restrictions

**Expected Behavior:**
- Fail immediately (do not retry)
- Distinguish between different 403 subtypes based on error.type

**Error Mapping:**
- Organization restriction: `IK_ERR_CAT_AUTH` (insufficient permissions)
- Geographic restriction: `IK_ERR_CAT_AUTH` (region blocked)
- Quota/billing: `IK_ERR_CAT_RATE_LIMIT` (payment required)

**OpenAI Error Types for 403:**
- `invalid_request_error` - Typically permission/access issues
- `billing_not_active` - Account has insufficient credits

**User-Facing Message Format:**
```
Permission denied: [message from API]
```
or
```
Quota exceeded: Your OpenAI account has insufficient credits.
Visit https://platform.openai.com/account/billing
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/openai/error_forbidden.jsonl`
- Response body includes error.type to distinguish subtypes:
  ```json
  {
    "error": {
      "message": "You do not have access to this model.",
      "type": "invalid_request_error",
      "param": null,
      "code": "model_not_found"
    }
  }
  ```

**Test Coverage:**
- Parse error.type to distinguish permission vs quota
- Map to correct IK_ERR_CAT_* category based on type and code
- Extract and format error message

### 429 Too Many Requests

**Scenario:** Rate limiting applied (requests per minute, tokens per minute, or daily quota)

**Expected Behavior:**
- Extract retry-after delay from headers or response body
- Return error with retry guidance (caller decides whether to retry)
- Do NOT automatically retry within provider

**Error Mapping:**
- Internal code: `IK_ERR_CAT_RATE_LIMIT`
- Include `retry_after_ms` in error context

**OpenAI Rate Limit Headers:**
```
x-ratelimit-limit-requests: 10000
x-ratelimit-remaining-requests: 0
x-ratelimit-reset-requests: 8.64s
x-ratelimit-limit-tokens: 200000
x-ratelimit-remaining-tokens: 15000
x-ratelimit-reset-tokens: 6.2s
retry-after: 9
```

**OpenAI Rate Limit Response:**
```json
{
  "error": {
    "message": "Rate limit reached for requests",
    "type": "requests",
    "param": null,
    "code": "rate_limit_exceeded"
  }
}
```

**User-Facing Message Format:**
```
Rate limit exceeded. Retry after 9 seconds.
Requests: 0/10000 remaining (resets in 8.64s)
Tokens: 15000/200000 remaining (resets in 6.2s)
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/openai/error_rate_limit.jsonl`
- Must include rate limit headers (see above)
- Response body includes error.type = "requests" or "tokens"

**Test Coverage:**
- Parse retry-after header (integer seconds)
- Parse x-ratelimit-* headers (requests and tokens)
- Extract reset durations (e.g., "8.64s")
- Map to IK_ERR_CAT_RATE_LIMIT with retry_after_ms
- Format user message with quota details
- Handle both "requests" and "tokens" limit types

### 500 Internal Server Error

**Scenario:** OpenAI server error (unexpected)

**Expected Behavior:**
- Fail immediately (do not retry within provider)
- Log full error for debugging
- Return generic server error to user

**Error Mapping:**
- Internal code: `IK_ERR_CAT_SERVER` (provider-side failure)

**User-Facing Message Format:**
```
OpenAI API error (500): Internal server error.
This is a temporary issue on OpenAI's side. Please retry later.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/openai/error_500.jsonl`
- May have error JSON or plain text body
- Status code 500 is primary signal

**Test Coverage:**
- Handle 500 status even if response body is empty or plain text
- Map to IK_ERR_CAT_SERVER
- Verify error message includes status code

### 503 Service Unavailable

**Scenario:** OpenAI service temporarily unavailable (maintenance, overload, or model loading)

**Expected Behavior:**
- Fail immediately (caller may retry with backoff)
- May include retry-after header
- Common during high load or when models are being loaded

**Error Mapping:**
- Internal code: `IK_ERR_CAT_SERVER`

**OpenAI 503 Response:**
```json
{
  "error": {
    "message": "The server is currently overloaded with other requests. Sorry about that! You can retry your request, or contact us through our help center at help.openai.com if the error persists.",
    "type": "server_error",
    "param": null,
    "code": "service_unavailable"
  }
}
```

**User-Facing Message Format:**
```
OpenAI service unavailable (503): The server is currently overloaded.
Retry after 30 seconds.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/openai/error_503.jsonl`
- May include retry-after header
- Response body includes error.code = "service_unavailable"

**Test Coverage:**
- Parse retry-after header if present
- Map to IK_ERR_CAT_SERVER
- Handle missing retry-after gracefully (use default backoff)
- Parse error.code field

### 504 Gateway Timeout

**Scenario:** Request timed out at OpenAI's gateway (long-running request)

**Expected Behavior:**
- Fail immediately
- Distinguish from local timeout (curl timeout)
- Common with o-series models or very large contexts

**Error Mapping:**
- Internal code: `IK_ERR_CAT_TIMEOUT` (provider gateway timeout)

**User-Facing Message Format:**
```
Request timed out (504): The API gateway did not receive a response in time.
This may occur with o-series models or very large contexts. Consider simplifying the request.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/openai/error_504.jsonl`
- Status code 504 with minimal or no body
- May include plain text: "Gateway timeout"

**Test Coverage:**
- Map 504 to IK_ERR_CAT_TIMEOUT
- Verify distinct from curl-level timeout (CURLE_OPERATION_TIMEDOUT)
- Handle both JSON error and plain text body
- Include timeout guidance in error message

### Error Mapping Summary Table

| HTTP Status | OpenAI Error Code | Internal Code | Retry? | Retry-After Header? |
|-------------|-------------------|---------------|--------|---------------------|
| 401 | invalid_api_key | IK_ERR_CAT_AUTH | No | No |
| 403 (permission) | model_not_found | IK_ERR_CAT_AUTH | No | No |
| 403 (billing) | billing_not_active | IK_ERR_CAT_RATE_LIMIT | No | No |
| 429 (requests) | rate_limit_exceeded | IK_ERR_CAT_RATE_LIMIT | Caller decides | Yes (required) |
| 429 (tokens) | rate_limit_exceeded | IK_ERR_CAT_RATE_LIMIT | Caller decides | Yes (required) |
| 500 | N/A | IK_ERR_CAT_SERVER | No | No |
| 503 | service_unavailable | IK_ERR_CAT_SERVER | Caller decides | Maybe |
| 504 | N/A | IK_ERR_CAT_TIMEOUT | No | No |

### OpenAI-Specific Error Handling Notes

1. **Authentication Header:** OpenAI uses `Authorization: Bearer <key>` header (not x-api-key). VCR fixtures must redact this.

2. **Error Structure:** OpenAI uses a consistent error envelope:
   ```json
   {
     "error": {
       "message": "<human-readable message>",
       "type": "<error category>",
       "param": "<parameter that caused error, if applicable>",
       "code": "<error code>"
     }
   }
   ```

3. **Error Types:** Common error.type values:
   - `invalid_request_error` - Client-side errors (auth, validation, etc.)
   - `invalid_api_key` - Authentication failure
   - `rate_limit_error` - Deprecated, now uses "requests" or "tokens"
   - `server_error` - OpenAI-side failures
   - `insufficient_quota` - Billing/quota issues

4. **Rate Limit Headers:** OpenAI provides detailed headers with both limits and reset times. Parse both requests and tokens limits.

5. **Rate Limit Types:** OpenAI has multiple rate limit dimensions:
   - Requests per minute (RPM)
   - Tokens per minute (TPM)
   - Tokens per day (TPD)
   - Each has separate limits and reset times

6. **Model-Specific Behavior:**
   - o-series models are more likely to timeout (504) due to extended reasoning
   - Different models have different rate limits (tracked via headers)

7. **Async Delivery:** All errors are delivered via completion callback, consistent with async architecture.

8. **VCR Recording:** When recording fixtures with VCR_RECORD=1:
   - Redact API key from Authorization header (replace with "sk-****")
   - Capture full rate limit headers
   - Test different error.code values to ensure proper mapping

9. **Mock Testing:** Mock curl_multi must simulate both HTTP status, headers, and OpenAI error JSON structure.

10. **Context Length Errors:** 400 errors with code "context_length_exceeded" are common and should be mapped to `IK_ERR_CAT_INVALID_ARG` with specific guidance about reducing context.

## Postconditions

- [ ] 3 test files created with 27+ tests total
- [ ] 5 fixture files recorded with VCR_RECORD=1 (JSONL format)
- [ ] No API keys in fixtures (verify: `grep -r "sk-" tests/fixtures/vcr/openai/` returns empty)
- [ ] Async tests use fdset/perform/info_read pattern (not blocking send)
- [ ] Mock curl_multi functions used (not curl_easy)
- [ ] Callbacks receive responses via ik_provider_completion_t
- [ ] Both GPT and o-series models tested
- [ ] Reasoning effort mapping tested
- [ ] Tool call argument parsing tested
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-openai-basic.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).