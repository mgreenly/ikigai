# Task: Create Anthropic Provider Basic Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** anthropic-core.md, anthropic-request.md, anthropic-response.md, vcr-core.md, vcr-mock-integration.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking:
- Use curl_multi (NOT curl_easy)
- Mock `curl_multi_*` functions, not `curl_easy_perform()`
- Test the async fdset/perform/info_read pattern
- NEVER use blocking calls in tests

Reference: `scratch/plan/testing-strategy.md`

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/anthropic/` - Anthropic provider implementation
- `tests/helpers/mock_http.h` - Mock curl_multi infrastructure
- `src/openai/client_multi.c` - Reference async pattern (existing)

**Plan:**
- `scratch/plan/testing-strategy.md` - Async test patterns, mock curl_multi approach
- `scratch/plan/provider-interface.md` - Vtable with fdset/perform/info_read methods

## Objective

Create tests for Anthropic provider adapter, request serialization, response parsing, and error handling. Uses mock curl_multi infrastructure for isolated async testing.

## Async Test Pattern

Tests must simulate the async fdset/perform/info_read cycle. The mock pattern:

```c
// Test captures response via completion callback
static ik_provider_completion_t *captured_completion;
static res_t captured_result;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx) {
    captured_completion = completion;
    return OK(NULL);
}

START_TEST(test_non_streaming_request)
{
    captured_completion = NULL;

    // Setup: Load fixture data, configure mock curl_multi
    const char *response_json = load_fixture("anthropic/response_basic.json");
    mock_set_response(200, response_json);

    // Create provider
    res_t r = ik_provider_get_or_create(ctx, "anthropic", &provider);
    ck_assert(is_ok(&r));

    // Start request with callback (returns immediately)
    r = provider->vt->start_request(provider->ctx, req, test_completion_cb, NULL);
    ck_assert(is_ok(&r));

    // Drive event loop until complete
    while (captured_completion == NULL) {
        fd_set read_fds, write_fds, exc_fds;
        int max_fd = 0;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&exc_fds);

        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);

        // In tests, mock perform() delivers data to callbacks
        int running = 1;
        while (running > 0) {
            provider->vt->perform(provider->ctx, &running);
        }
        provider->vt->info_read(provider->ctx, NULL);
    }

    // Assert on captured completion
    ck_assert(captured_completion->success);
    // ... validate response content
}
END_TEST
```

## Recording Fixtures

Before tests can run in playback mode, fixtures must be recorded from real API responses:

1. **Ensure valid credentials** - `ANTHROPIC_API_KEY` environment variable must be set
2. **Run in record mode**:
   ```bash
   VCR_RECORD=1 make build/tests/unit/providers/anthropic/test_anthropic_adapter
   VCR_RECORD=1 ./build/tests/unit/providers/anthropic/test_anthropic_adapter
   ```
3. **Verify fixtures created** - Check `tests/fixtures/vcr/anthropic/*.jsonl` files exist
4. **Verify no credentials leaked** - `grep -r "sk-ant-" tests/fixtures/vcr/anthropic/` returns nothing
5. **Commit fixtures** - Fixtures are committed to git for deterministic CI runs

**Note:** Fixtures only need re-recording when API behavior changes. Normal test runs use playback mode (VCR_RECORD unset).

## Interface

**Test files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/anthropic/test_anthropic_adapter.c` | Provider vtable implementation (async methods) |
| `tests/unit/providers/anthropic/test_anthropic_client.c` | Request serialization to Messages API |
| `tests/unit/providers/anthropic/test_anthropic_errors.c` | Error response parsing and mapping |

**Fixture files (VCR JSONL format):**

| File | Purpose |
|------|---------|
| `tests/fixtures/vcr/anthropic/response_basic.jsonl` | Standard completion response |
| `tests/fixtures/vcr/anthropic/response_thinking.jsonl` | Response with thinking content block |
| `tests/fixtures/vcr/anthropic/response_tool_call.jsonl` | Response with tool use content block |
| `tests/fixtures/vcr/anthropic/error_auth.jsonl` | 401 authentication error |
| `tests/fixtures/vcr/anthropic/error_rate_limit.jsonl` | 429 rate limit error (synthetic) |

## Test Scenarios

**Adapter Tests (6 tests) - Async Pattern:**
- Create adapter with valid credentials
- Destroy adapter cleans up resources (including curl_multi handle)
- start_request() returns immediately (non-blocking)
- Completion callback invoked after fdset/perform/info_read cycle
- start_request() returns ERR via callback on HTTP failure
- All vtable functions are non-NULL (fdset, perform, timeout, info_read, start_request, start_stream)

**Request Serialization Tests (6 tests):**
- Build request with system and user messages
- Build request with thinking budget (extended thinking)
- Build request with tool definitions
- Build request without optional fields
- Verify correct headers (API key, version, content-type)
- Verify JSON structure matches Messages API spec

**Response Parsing Tests (5 tests):**
- Parse response with single text block
- Parse response with thinking block followed by text block
- Parse response with tool use block
- Parse response with multiple content blocks
- Extract usage metadata (tokens)

**Error Handling Tests (5 tests):**
- Parse authentication error (401) - delivered via completion callback
- Parse rate limit error (429) - includes retry_after_ms extraction
- Parse overloaded error (529)
- Parse validation error (400)
- Map errors to correct ERR_* categories

## HTTP Status Code Error Matrix

This matrix defines how each HTTP status code should be handled, mapped to internal errors, and presented to users. All errors are delivered asynchronously via completion callbacks.

### 401 Unauthorized

**Scenario:** Invalid or expired API key

**Expected Behavior:**
- Fail immediately (do not retry)
- Error delivered via completion callback with `success = false`

**Error Mapping:**
- Internal code: `ERR_AUTH`
- Category: Authentication failure

**User-Facing Message Format:**
```
Authentication failed: Invalid or expired API key.
Please verify your ANTHROPIC_API_KEY is correct.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/anthropic/error_auth.jsonl`
- Response body must include Anthropic error JSON:
  ```json
  {
    "type": "error",
    "error": {
      "type": "authentication_error",
      "message": "invalid x-api-key"
    }
  }
  ```
- Fixture must redact actual API key from request headers

**Test Coverage:**
- Parse error type and message from response body
- Map to ERR_AUTH
- Verify callback receives error (not thrown exception)

### 403 Forbidden

**Scenario:** Permission denied or quota/credits exhausted

**Expected Behavior:**
- Fail immediately (do not retry)
- Distinguish between permission errors and quota errors

**Error Mapping:**
- Permission denied: `ERR_AUTH` (insufficient permissions)
- Quota exhausted: `ERR_QUOTA` (credits exhausted)

**User-Facing Message Format:**
```
Permission denied: [error message from API]
```
or
```
Quota exceeded: Your API credits have been exhausted.
Visit https://console.anthropic.com to add credits.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/anthropic/error_forbidden.jsonl`
- Response body includes error type to distinguish permission vs quota:
  ```json
  {
    "type": "error",
    "error": {
      "type": "permission_error"
    }
  }
  ```

**Test Coverage:**
- Parse permission_error vs quota error types
- Map to correct ERR_* category based on error type
- Extract message from response

### 429 Too Many Requests

**Scenario:** Rate limiting applied

**Expected Behavior:**
- Extract retry-after delay from headers or response body
- Return error with retry guidance (caller decides whether to retry)
- Do NOT automatically retry within provider

**Error Mapping:**
- Internal code: `ERR_RATE_LIMIT`
- Include `retry_after_ms` in error context

**Anthropic Rate Limit Headers:**
```
anthropic-ratelimit-requests-limit: 1000
anthropic-ratelimit-requests-remaining: 0
anthropic-ratelimit-requests-reset: 2025-12-23T12:34:56Z
anthropic-ratelimit-tokens-limit: 100000
anthropic-ratelimit-tokens-remaining: 5000
anthropic-ratelimit-tokens-reset: 2025-12-23T12:35:00Z
retry-after: 60
```

**User-Facing Message Format:**
```
Rate limit exceeded. Retry after 60 seconds.
Requests: 0/1000 remaining (resets at 2025-12-23T12:34:56Z)
Tokens: 5000/100000 remaining
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/anthropic/error_rate_limit.jsonl`
- Must include rate limit headers (see above)
- Response body:
  ```json
  {
    "type": "error",
    "error": {
      "type": "rate_limit_error",
      "message": "Rate limit exceeded"
    }
  }
  ```

**Test Coverage:**
- Parse retry-after header (integer seconds)
- Parse rate limit headers (requests and tokens)
- Extract reset timestamps
- Map to ERR_RATE_LIMIT with retry_after_ms
- Format user message with quota details

### 500 Internal Server Error

**Scenario:** Anthropic server error (unexpected)

**Expected Behavior:**
- Fail immediately (do not retry within provider)
- Log full error for debugging
- Return generic server error to user

**Error Mapping:**
- Internal code: `ERR_PROVIDER` (provider-side failure)

**User-Facing Message Format:**
```
Anthropic API error (500): Internal server error.
This is a temporary issue on Anthropic's side. Please retry later.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/anthropic/error_500.jsonl`
- May have empty body or generic error JSON
- Status code 500 is primary signal

**Test Coverage:**
- Handle 500 status even if response body is empty
- Map to ERR_PROVIDER
- Verify error message includes status code

### 503 Service Unavailable

**Scenario:** Anthropic service overloaded or in maintenance

**Expected Behavior:**
- Fail immediately (caller may retry with backoff)
- May include retry-after header

**Error Mapping:**
- Internal code: `ERR_OVERLOADED`

**User-Facing Message Format:**
```
Anthropic service unavailable (503): [message from response]
The service may be temporarily overloaded. Retry after 30 seconds.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/anthropic/error_503.jsonl`
- May include retry-after header
- Response body:
  ```json
  {
    "type": "error",
    "error": {
      "type": "overloaded_error",
      "message": "Service is temporarily overloaded"
    }
  }
  ```

**Test Coverage:**
- Parse retry-after if present
- Map to ERR_OVERLOADED
- Handle missing retry-after gracefully

### 504 Gateway Timeout

**Scenario:** Request timed out at Anthropic's gateway (long-running request)

**Expected Behavior:**
- Fail immediately
- Distinguish from local timeout (curl timeout)

**Error Mapping:**
- Internal code: `ERR_TIMEOUT` (provider gateway timeout)

**User-Facing Message Format:**
```
Request timed out (504): The API gateway did not receive a response in time.
This may occur with very long or complex requests. Consider simplifying the request.
```

**VCR Fixture Requirements:**
- File: `tests/fixtures/vcr/anthropic/error_504.jsonl`
- Status code 504 with minimal body

**Test Coverage:**
- Map 504 to ERR_TIMEOUT
- Verify distinct from curl-level timeout (CURLE_OPERATION_TIMEDOUT)
- Include timeout guidance in error message

### Additional Anthropic-Specific Errors

**529 Overloaded (Anthropic-specific):**
- Similar to 503, but Anthropic's custom code
- Map to `ERR_OVERLOADED`
- Already covered in existing test scenarios

### Error Mapping Summary Table

| HTTP Status | Error Type | Internal Code | Retry? | Retry-After Header? |
|-------------|------------|---------------|--------|---------------------|
| 401 | Authentication | ERR_AUTH | No | No |
| 403 (permission) | Permission denied | ERR_AUTH | No | No |
| 403 (quota) | Quota exhausted | ERR_QUOTA | No | No |
| 429 | Rate limit | ERR_RATE_LIMIT | Caller decides | Yes (required) |
| 500 | Server error | ERR_PROVIDER | No | No |
| 503 | Overloaded | ERR_OVERLOADED | Caller decides | Maybe |
| 504 | Gateway timeout | ERR_TIMEOUT | No | No |
| 529 | Overloaded (custom) | ERR_OVERLOADED | Caller decides | Maybe |

### Implementation Notes

1. **Async Delivery:** All errors are delivered via completion callback, never thrown synchronously
2. **Header Parsing:** Rate limit headers are provider-specific (see Anthropic Rate Limit Headers above)
3. **Retry Logic:** Provider does NOT implement retry logic; caller (agent loop) handles retries based on error code
4. **VCR Recording:** When recording fixtures with VCR_RECORD=1:
   - Use real API to get authentic error responses (may need to trigger intentionally)
   - Verify headers are captured in JSONL format
   - Redact API keys from both request headers and response bodies
5. **Mock Testing:** Mock curl_multi must simulate both HTTP status and response body delivery

## Mock curl_multi Functions

Tests mock these curl_multi wrappers (defined in tests/helpers/mock_http.h):

| Mock Function | Purpose |
|---------------|---------|
| `curl_multi_fdset_()` | Returns mock FDs (can return -1 for no FDs) |
| `curl_multi_perform_()` | Simulates progress, invokes write callbacks with mock data |
| `curl_multi_info_read_()` | Returns completion messages with mock HTTP status |
| `curl_multi_timeout_()` | Returns recommended timeout |

The mock infrastructure maintains:
- Queued responses (status code + body)
- Simulated transfer state
- Callback invocation on perform()

## Postconditions

- [ ] 3 test files created with 22+ tests total
- [ ] 5 fixture files recorded with VCR_RECORD=1 (JSONL format)
- [ ] No API keys in fixtures (verify: `grep -r "sk-ant-" tests/fixtures/vcr/anthropic/` returns empty)
- [ ] All tests use mock curl_multi (no real network, no blocking calls)
- [ ] Tests exercise fdset/perform/info_read cycle
- [ ] Responses delivered via completion callbacks
- [ ] Error mapping covers key HTTP status codes
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-anthropic-basic.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).