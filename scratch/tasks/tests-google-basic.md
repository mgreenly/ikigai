# Task: Create Google Provider Basic Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** google-core.md, google-request.md, google-response.md, vcr-core.md, vcr-mock-integration.md

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
3. **Verify fixtures created** - Check `tests/fixtures/google/*.jsonl` files exist
4. **Verify no credentials leaked** - `grep -r "AIza" tests/fixtures/google/` returns nothing
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
| `tests/fixtures/google/response_basic.jsonl` | Standard completion response |
| `tests/fixtures/google/response_thinking.jsonl` | Response with thought=true parts |
| `tests/fixtures/google/response_function_call.jsonl` | Response with functionCall |
| `tests/fixtures/google/error_auth.jsonl` | 401/403 authentication error |
| `tests/fixtures/google/error_rate_limit.jsonl` | 429 rate limit error (synthetic) |

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
    res_t r = ik_google_create(ctx, "test-key", &provider);
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
- [ ] No API keys in fixtures (verify: `grep -r "AIza" tests/fixtures/google/` returns empty)
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