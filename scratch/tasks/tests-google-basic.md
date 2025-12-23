# Task: Create Google Provider Basic Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** google-core.md, google-request.md, google-response.md, vcr-core.md, vcr-mock-integration.md, vcr-fixtures-setup.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

**Critical Architecture Constraint:** The application uses a select()-based event loop. ALL HTTP operations are non-blocking via curl_multi. Tests MUST simulate the async fdset/perform/info_read cycle, not blocking calls.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Plan:**
- `scratch/plan/testing-strategy.md` - Mock HTTP pattern, async test flow
- `scratch/plan/provider-interface.md` - Async vtable specification

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/google/` - Google/Gemini provider implementation
- `tests/helpers/mock_http.h` - Mock infrastructure (curl_multi mocks)

## Objective

Create tests for Google Gemini provider adapter, request serialization, response parsing, and error handling. Covers both Gemini 2.5 (thinkingBudget) and Gemini 3.0 (thinkingLevel) thinking parameters.

## Recording Fixtures

Before tests can run in playback mode, fixtures must be recorded from real API responses:

1. **Ensure valid credentials** - `GOOGLE_API_KEY` environment variable must be set
2. **Run in record mode**:
   ```bash
   VCR_RECORD=1 make build/tests/unit/providers/google/test_google_adapter
   VCR_RECORD=1 ./build/tests/unit/providers/google/test_google_adapter
   ```
3. **Verify fixtures created** - Check `tests/fixtures/vcr/google/*.jsonl` files exist
4. **Verify no credentials leaked** - `grep -r "AIza" tests/fixtures/vcr/google/` returns nothing
5. **Commit fixtures** - Fixtures are committed to git for deterministic CI runs

**Note:** Fixtures only need re-recording when API behavior changes. Normal test runs use playback mode (VCR_RECORD unset).

## Interface

**Test files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/google/test_google_adapter.c` | Provider vtable implementation |
| `tests/unit/providers/google/test_google_client.c` | Request serialization to Gemini API |
| `tests/unit/providers/google/test_google_errors.c` | Error response parsing and mapping |

**Fixture files (VCR JSONL format):**

| File | Purpose |
|------|---------|
| `tests/fixtures/vcr/google/response_basic.jsonl` | Standard completion response |
| `tests/fixtures/vcr/google/response_thinking.jsonl` | Response with thought=true parts |
| `tests/fixtures/vcr/google/response_function_call.jsonl` | Response with functionCall |
| `tests/fixtures/vcr/google/error_auth.jsonl` | 401/403 authentication error |
| `tests/fixtures/vcr/google/error_rate_limit.jsonl` | 429 rate limit error (synthetic) |

## Test Scenarios

**Adapter Tests (5 tests):**
- Create adapter with valid credentials
- Destroy adapter cleans up resources
- Start request + drive event loop returns response via callback
- Start request returns ERR on HTTP failure (via callback)
- Vtable functions are non-NULL (fdset, perform, timeout, info_read, start_request, start_stream)

**Request Serialization Tests (7 tests):**
- Build request with system and user messages
- Build request for Gemini 2.5 with thinkingBudget
- Build request for Gemini 3.0 with thinkingLevel
- Build request with tool declarations
- Build request without optional fields
- Verify API key in URL (not header)
- Verify JSON structure matches Gemini API spec

**Response Parsing Tests (6 tests):**
- Parse response with single text part
- Parse response with thought=true part followed by text
- Parse response with functionCall part
- Parse response with multiple parts
- Generate UUID for tool call (Gemini doesn't provide IDs)
- Detect thought signature in text content

**Error Handling Tests (5 tests):**
- Parse authentication error (401/403)
- Parse rate limit error (429)
- Parse quota exceeded error
- Parse validation error (400)
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
Please verify your GOOGLE_API_KEY is correct.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/google/error_auth.jsonl`
- Response body must include Google error JSON:
  ```json
  {
    "error": {
      "code": 401,
      "message": "API key not valid. Please pass a valid API key.",
      "status": "UNAUTHENTICATED"
    }
  }
  ```
- Fixture must redact actual API key from request URL (key query parameter)

**Test Coverage:**
- Parse error.code and error.message from response body
- Map to IK_ERR_CAT_AUTH
- Verify callback receives error (not thrown exception)

### 403 Forbidden

**Scenario:** Permission denied, API not enabled, or quota exhausted

**Expected Behavior:**
- Fail immediately (do not retry)
- Distinguish between different 403 subtypes based on error.status

**Error Mapping:**
- API not enabled: `IK_ERR_CAT_AUTH` (configuration error)
- Permission denied: `IK_ERR_CAT_AUTH` (insufficient permissions)
- Quota exceeded: `IK_ERR_CAT_QUOTA` (billing/quota issue)

**Google Error Status Values:**
- `PERMISSION_DENIED` - API not enabled or insufficient permissions
- `RESOURCE_EXHAUSTED` - Quota exceeded

**User-Facing Message Format:**
```
Permission denied: Gemini API is not enabled for this project.
Visit https://console.cloud.google.com/apis/library/generativelanguage.googleapis.com
```
or
```
Quota exceeded: [message from API]
Visit https://console.cloud.google.com/apis/api/generativelanguage.googleapis.com/quotas
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/google/error_forbidden.jsonl`
- Response body includes error.status to distinguish subtypes:
  ```json
  {
    "error": {
      "code": 403,
      "message": "Gemini API has not been used in project...",
      "status": "PERMISSION_DENIED"
    }
  }
  ```

**Test Coverage:**
- Parse error.status field to distinguish permission vs quota
- Map to correct ERR_* category based on status
- Extract and format error message

### 429 Too Many Requests

**Scenario:** Rate limiting applied (requests per minute or tokens per minute)

**Expected Behavior:**
- Extract retry-after delay if present
- Return error with retry guidance (caller decides whether to retry)
- Do NOT automatically retry within provider

**Error Mapping:**
- Internal code: `IK_ERR_CAT_RATE_LIMIT`
- Include `retry_after_ms` in error context if available

**Google Rate Limit Response:**
```json
{
  "error": {
    "code": 429,
    "message": "Resource has been exhausted (e.g. check quota).",
    "status": "RESOURCE_EXHAUSTED",
    "details": [
      {
        "@type": "type.googleapis.com/google.rpc.ErrorInfo",
        "reason": "RATE_LIMIT_EXCEEDED",
        "domain": "googleapis.com",
        "metadata": {
          "service": "generativelanguage.googleapis.com",
          "quota_limit": "RequestsPerMinutePerProject"
        }
      }
    ]
  }
}
```

**User-Facing Message Format:**
```
Rate limit exceeded: RequestsPerMinutePerProject quota exhausted.
Retry after 60 seconds.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/google/error_rate_limit.jsonl`
- May include Retry-After header (integer seconds)
- Response body includes error.details with quota information

**Test Coverage:**
- Parse Retry-After header if present
- Parse error.details for quota_limit and reason
- Map to IK_ERR_CAT_RATE_LIMIT with retry_after_ms
- Format user message with quota details

**Note:** Google does not provide detailed rate limit headers like Anthropic. Quota information is in error.details.

### 500 Internal Server Error

**Scenario:** Google server error (unexpected)

**Expected Behavior:**
- Fail immediately (do not retry within provider)
- Log full error for debugging
- Return generic server error to user

**Error Mapping:**
- Internal code: `IK_ERR_CAT_SERVER` (provider-side failure)

**User-Facing Message Format:**
```
Google API error (500): Internal server error.
This is a temporary issue on Google's side. Please retry later.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/google/error_500.jsonl`
- May have error JSON or empty body
- Status code 500 is primary signal

**Test Coverage:**
- Handle 500 status even if response body is empty or malformed
- Map to IK_ERR_CAT_SERVER
- Verify error message includes status code

### 503 Service Unavailable

**Scenario:** Google service temporarily unavailable (maintenance or overload)

**Expected Behavior:**
- Fail immediately (caller may retry with backoff)
- May include Retry-After header

**Error Mapping:**
- Internal code: `IK_ERR_CAT_SERVER`

**User-Facing Message Format:**
```
Google service unavailable (503): The service is temporarily unavailable.
Retry after 30 seconds.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/google/error_503.jsonl`
- May include Retry-After header
- Response body:
  ```json
  {
    "error": {
      "code": 503,
      "message": "The service is currently unavailable.",
      "status": "UNAVAILABLE"
    }
  }
  ```

**Test Coverage:**
- Parse Retry-After header if present
- Map to IK_ERR_CAT_SERVER
- Handle missing Retry-After gracefully (use default backoff)

### 504 Gateway Timeout

**Scenario:** Request timed out at Google's gateway (long-running request)

**Expected Behavior:**
- Fail immediately
- Distinguish from local timeout (curl timeout)

**Error Mapping:**
- Internal code: `IK_ERR_CAT_TIMEOUT` (provider gateway timeout)

**User-Facing Message Format:**
```
Request timed out (504): The API gateway did not receive a response in time.
This may occur with very long or complex requests. Consider simplifying the request.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/google/error_504.jsonl`
- Status code 504 with minimal or no body

**Test Coverage:**
- Map 504 to IK_ERR_CAT_TIMEOUT
- Verify distinct from curl-level timeout (CURLE_OPERATION_TIMEDOUT)
- Include timeout guidance in error message

### Error Mapping Summary Table

| HTTP Status | Google Status | Internal Code | Retry? | Retry-After Header? |
|-------------|---------------|---------------|--------|---------------------|
| 401 | UNAUTHENTICATED | IK_ERR_CAT_AUTH | No | No |
| 403 (API disabled) | PERMISSION_DENIED | IK_ERR_CAT_AUTH | No | No |
| 403 (quota) | RESOURCE_EXHAUSTED | IK_ERR_CAT_QUOTA | No | No |
| 429 | RESOURCE_EXHAUSTED | IK_ERR_CAT_RATE_LIMIT | Caller decides | Maybe |
| 500 | INTERNAL | IK_ERR_CAT_SERVER | No | No |
| 503 | UNAVAILABLE | IK_ERR_CAT_SERVER | Caller decides | Maybe |
| 504 | DEADLINE_EXCEEDED | IK_ERR_CAT_TIMEOUT | No | No |

### Google-Specific Error Handling Notes

1. **API Key in URL:** Google passes API key as query parameter (`?key=...`), not in headers. VCR fixtures must redact this.

2. **Error Structure:** Google uses a consistent error envelope:
   ```json
   {
     "error": {
       "code": <HTTP status code>,
       "message": "<human-readable message>",
       "status": "<gRPC status code>",
       "details": [...]  // Optional structured details
     }
   }
   ```

3. **Status Codes:** Google includes both HTTP status code (error.code) and gRPC status (error.status). The gRPC status provides more semantic information.

4. **Rate Limit Details:** Unlike Anthropic, Google doesn't provide quota headers in responses. Rate limit details are only in error.details when limit is exceeded.

5. **Quota Types:** Google has multiple quota dimensions:
   - RequestsPerMinutePerProject
   - RequestsPerMinutePerUser (if using OAuth)
   - GenerateContentRequestsPerMinute
   - TokensPerMinute

6. **Async Delivery:** All errors are delivered via completion callback, consistent with async architecture.

7. **VCR Recording:** When recording fixtures with VCR_RECORD=1:
   - Redact API key from request URL query parameters
   - Capture full error.details for quota information
   - Test both error.status and HTTP status code handling

8. **Mock Testing:** Mock curl_multi must simulate both HTTP status and Google error JSON structure.

## Async Test Pattern

Tests MUST simulate the async event loop. Use this pattern (from `scratch/plan/testing-strategy.md`):

```c
// Test captures response via callback
static ik_response_t *captured_response;
static res_t captured_result;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx) {
    captured_result = completion->result;
    captured_response = completion->response;
    return OK(NULL);
}

START_TEST(test_non_streaming_request)
{
    captured_response = NULL;

    // Setup: Load fixture data
    const char *response_json = load_fixture("google/response_basic.json");
    mock_set_response(200, response_json);

    // Create provider
    res_t r = ik_provider_get_or_create(ctx, "google", &provider);
    ck_assert(is_ok(&r));

    // Start request with callback (returns immediately)
    r = provider->vt->start_request(provider->ctx, req, test_completion_cb, NULL);
    ck_assert(is_ok(&r));

    // Drive event loop until complete
    while (captured_response == NULL) {
        fd_set read_fds, write_fds, exc_fds;
        int max_fd = 0;
        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
        select(max_fd + 1, &read_fds, &write_fds, &exc_fds, NULL);
        provider->vt->perform(provider->ctx, NULL);
    }

    // Assert on captured response
    ck_assert(is_ok(&captured_result));
    ck_assert_str_eq(captured_response->content, "expected");
}
END_TEST
```

**Mock curl_multi functions (from mock_http.h):**
- `curl_multi_fdset_()` - Returns mock FDs
- `curl_multi_perform_()` - Simulates progress, delivers data to callbacks
- `curl_multi_info_read_()` - Returns completion messages

## Postconditions

- [ ] 3 test files created with 23+ tests total
- [ ] 5 fixture files recorded with VCR_RECORD=1 (JSONL format)
- [ ] No API keys in fixtures (verify: `grep -r "AIza" tests/fixtures/vcr/google/` returns empty)
- [ ] Both Gemini 2.5 and 3.0 thinking params tested
- [ ] UUID generation for tool calls tested
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-google-basic.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).