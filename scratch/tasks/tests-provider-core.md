# Task: Create Provider Core Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** provider-factory.md, error-core.md, credentials-core.md, vcr-core.md, vcr-mock-integration.md, vcr-fixtures-setup.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

**Critical Architecture Constraint:** The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking. Tests must verify the async fdset/perform/info_read pattern, NOT blocking send/stream calls.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load errors` - Error handling patterns

**Source:**
- `src/providers/factory.c` - Provider factory implementation
- `src/providers/common/error.c` - Error handling implementation
- `src/credentials.c` - Credentials API
- `src/openai/client_multi.c` - Reference async pattern (fdset/perform/info_read)

**Plan:**
- `scratch/plan/testing-strategy.md` - Async test patterns, mock curl_multi
- `scratch/plan/provider-interface.md` - Async vtable specification
- `scratch/plan/architecture.md` - Event loop integration

## Objective

Create unit tests for provider creation, error category handling, credentials lookup, and async vtable operations. These tests verify:
1. Provider creation via factory
2. Error category mapping and retryability
3. Credentials lookup from environment
4. **Async event loop integration (fdset/perform/info_read pattern)**

The async vtable tests are critical - they verify providers integrate correctly with the select()-based event loop.

## Interface

**Files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/test_provider_factory.c` | Provider creation and async vtable tests |
| `tests/unit/providers/common/test_error_handling.c` | Error category tests |
| `tests/unit/test_credentials.c` | Credentials lookup tests |

## Behaviors

### Async Vtable Pattern (CRITICAL)

All providers must implement these async vtable methods for select() integration:

```c
// Event loop integration (from provider->vtable)
res_t (*fdset)(void *ctx, fd_set *r, fd_set *w, fd_set *e, int *max_fd);
res_t (*perform)(void *ctx, int *running_handles);
res_t (*timeout)(void *ctx, long *timeout_ms);
void  (*info_read)(void *ctx, ik_logger_t *logger);

// Non-blocking request initiation
res_t (*start_request)(void *ctx, const ik_request_t *req,
                       ik_provider_completion_cb_t cb, void *cb_ctx);
res_t (*start_stream)(void *ctx, const ik_request_t *req,
                      ik_stream_cb_t stream_cb, void *stream_ctx,
                      ik_provider_completion_cb_t completion_cb, void *completion_ctx);
```

**Test flow for async requests:**
1. Call `start_request()` or `start_stream()` - returns immediately
2. Loop: call `fdset()` to get FDs, `select()`, then `perform()`
3. Call `info_read()` to check for completed transfers and invoke callbacks
4. Repeat until callback is invoked

### Provider Factory Tests (`test_provider_factory.c`)

```c
// Capture callback results
static ik_provider_completion_t *captured_completion;
static bool callback_invoked;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    captured_completion = talloc_steal(ctx, completion);
    callback_invoked = true;
    return OK(NULL);
}

START_TEST(test_create_openai_provider)
{
    setenv("OPENAI_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider->name, "openai");
    ck_assert_ptr_nonnull(provider->vtable);

    // Verify async vtable methods exist (NOT blocking send/stream)
    ck_assert_ptr_nonnull(provider->vtable->fdset);
    ck_assert_ptr_nonnull(provider->vtable->perform);
    ck_assert_ptr_nonnull(provider->vtable->timeout);
    ck_assert_ptr_nonnull(provider->vtable->info_read);
    ck_assert_ptr_nonnull(provider->vtable->start_request);
    ck_assert_ptr_nonnull(provider->vtable->start_stream);

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_create_anthropic_provider)
{
    setenv("ANTHROPIC_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "anthropic", &provider);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(provider->name, "anthropic");

    // Verify async vtable methods
    ck_assert_ptr_nonnull(provider->vtable->fdset);
    ck_assert_ptr_nonnull(provider->vtable->perform);
    ck_assert_ptr_nonnull(provider->vtable->start_stream);

    unsetenv("ANTHROPIC_API_KEY");
}
END_TEST

START_TEST(test_create_google_provider)
{
    setenv("GOOGLE_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "google", &provider);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(provider->name, "google");

    // Verify async vtable methods
    ck_assert_ptr_nonnull(provider->vtable->fdset);
    ck_assert_ptr_nonnull(provider->vtable->perform);

    unsetenv("GOOGLE_API_KEY");
}
END_TEST

START_TEST(test_create_unknown_provider_fails)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "unknown", &provider);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_create_provider_missing_credentials)
{
    unsetenv("OPENAI_API_KEY");

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    ck_assert(is_err(&result));
    // Should indicate missing credentials
}
END_TEST

// Async event loop integration tests
START_TEST(test_provider_fdset_returns_ok)
{
    setenv("OPENAI_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);
    ck_assert(is_ok(&result));

    // fdset should work even with no active requests
    fd_set read_fds, write_fds, exc_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);
    int max_fd = 0;

    result = provider->vtable->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(is_ok(&result));

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_provider_perform_returns_ok)
{
    setenv("OPENAI_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);
    ck_assert(is_ok(&result));

    // perform should work even with no active requests
    int running_handles = -1;
    result = provider->vtable->perform(provider->ctx, &running_handles);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(running_handles, 0);  // No active requests

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_provider_timeout_returns_value)
{
    setenv("OPENAI_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);
    ck_assert(is_ok(&result));

    long timeout_ms = -999;
    result = provider->vtable->timeout(provider->ctx, &timeout_ms);
    ck_assert(is_ok(&result));
    // timeout should be >= -1 (curl convention: -1 means no timeout)
    ck_assert(timeout_ms >= -1);

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_provider_async_request_flow)
{
    // This test verifies the complete async flow with mock curl_multi
    setenv("OPENAI_API_KEY", "test-key", 1);

    // Setup mock to return fixture response
    const char *response_json = load_fixture("openai/response_basic.json");
    mock_set_response(200, response_json);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);
    ck_assert(is_ok(&result));

    // Create request
    ik_request_t *req = create_test_request(test_ctx);
    callback_invoked = false;
    captured_completion = NULL;

    // Start request (returns immediately - non-blocking)
    result = provider->vtable->start_request(provider->ctx, req,
                                              test_completion_cb, test_ctx);
    ck_assert(is_ok(&result));

    // Drive event loop until callback invoked
    int iterations = 0;
    while (!callback_invoked && iterations < 100) {
        fd_set read_fds, write_fds, exc_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&exc_fds);
        int max_fd = 0;

        provider->vtable->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);

        // In real code: select(max_fd + 1, &read_fds, &write_fds, &exc_fds, &timeout);
        // Mock doesn't need real select - perform() simulates progress

        int running;
        provider->vtable->perform(provider->ctx, &running);
        provider->vtable->info_read(provider->ctx, NULL);

        iterations++;
    }

    // Verify callback was invoked
    ck_assert(callback_invoked);
    ck_assert_ptr_nonnull(captured_completion);
    ck_assert(captured_completion->success);

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_provider_async_stream_flow)
{
    // Verify streaming uses async pattern
    setenv("OPENAI_API_KEY", "test-key", 1);

    // Setup mock to return SSE stream fixture
    const char *sse_data = load_fixture("openai/stream_basic.txt");
    mock_set_streaming_response(200, sse_data);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);
    ck_assert(is_ok(&result));

    ik_request_t *req = create_test_request(test_ctx);
    callback_invoked = false;

    // Track stream events
    static int stream_event_count = 0;
    stream_event_count = 0;

    // Start stream (returns immediately - non-blocking)
    result = provider->vtable->start_stream(provider->ctx, req,
                                             test_stream_cb, &stream_event_count,
                                             test_completion_cb, test_ctx);
    ck_assert(is_ok(&result));

    // Drive event loop
    int iterations = 0;
    while (!callback_invoked && iterations < 100) {
        int running;
        provider->vtable->perform(provider->ctx, &running);
        provider->vtable->info_read(provider->ctx, NULL);
        iterations++;
    }

    // Verify stream events were delivered during perform()
    ck_assert(stream_event_count > 0);
    ck_assert(callback_invoked);

    unsetenv("OPENAI_API_KEY");
}
END_TEST
```

### Error Handling Tests (`test_error_handling.c`)

```c
START_TEST(test_error_category_names)
{
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_AUTH), "authentication");
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_RATE_LIMIT), "rate_limit");
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_SERVICE), "service");
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_NETWORK), "network");
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_INVALID_REQUEST), "invalid_request");
}
END_TEST

START_TEST(test_error_is_retryable)
{
    ck_assert(ik_error_is_retryable(ERR_CAT_RATE_LIMIT));
    ck_assert(ik_error_is_retryable(ERR_CAT_SERVICE));
    ck_assert(ik_error_is_retryable(ERR_CAT_NETWORK));

    ck_assert(!ik_error_is_retryable(ERR_CAT_AUTH));
    ck_assert(!ik_error_is_retryable(ERR_CAT_INVALID_REQUEST));
}
END_TEST

START_TEST(test_http_status_to_category)
{
    ck_assert_int_eq(ik_http_status_to_category(401), ERR_CAT_AUTH);
    ck_assert_int_eq(ik_http_status_to_category(403), ERR_CAT_AUTH);
    ck_assert_int_eq(ik_http_status_to_category(429), ERR_CAT_RATE_LIMIT);
    ck_assert_int_eq(ik_http_status_to_category(500), ERR_CAT_SERVICE);
    ck_assert_int_eq(ik_http_status_to_category(502), ERR_CAT_SERVICE);
    ck_assert_int_eq(ik_http_status_to_category(503), ERR_CAT_SERVICE);
    ck_assert_int_eq(ik_http_status_to_category(400), ERR_CAT_INVALID_REQUEST);
}
END_TEST

START_TEST(test_error_user_message)
{
    const char *msg = ik_error_user_message(ERR_CAT_RATE_LIMIT);
    ck_assert_ptr_nonnull(msg);
    ck_assert(strstr(msg, "rate") != NULL || strstr(msg, "limit") != NULL);
}
END_TEST
```

### Credentials Tests (`test_credentials.c`)

```c
START_TEST(test_credentials_from_env_openai)
{
    setenv("OPENAI_API_KEY", "sk-test123", 1);

    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "openai", &key);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(key, "sk-test123");

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_credentials_from_env_anthropic)
{
    setenv("ANTHROPIC_API_KEY", "sk-ant-test", 1);

    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "anthropic", &key);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(key, "sk-ant-test");

    unsetenv("ANTHROPIC_API_KEY");
}
END_TEST

START_TEST(test_credentials_from_env_google)
{
    setenv("GOOGLE_API_KEY", "AIza-test", 1);

    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "google", &key);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(key, "AIza-test");

    unsetenv("GOOGLE_API_KEY");
}
END_TEST

START_TEST(test_credentials_missing_returns_error)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "openai", &key);

    ck_assert(is_err(&result));
}
END_TEST

START_TEST(test_credentials_unknown_provider)
{
    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "unknown_provider", &key);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST
```

## Test Scenarios

**Provider Factory (11 tests):**
1. Create OpenAI provider with async vtable methods
2. Create Anthropic provider with async vtable methods
3. Create Google provider with async vtable methods
4. Unknown provider returns error
5. Missing credentials returns error
6. fdset() returns OK with no active requests
7. perform() returns OK with running_handles=0
8. timeout() returns valid value
9. Complete async request flow (start_request -> perform loop -> callback)
10. Complete async stream flow (start_stream -> perform loop -> stream events -> callback)

**Error Handling (4 tests):**
1. Category names are correct
2. Retryable categories identified
3. HTTP status codes mapped correctly
4. User-friendly messages exist

**Credentials (5 tests):**
1. OpenAI key from environment
2. Anthropic key from environment
3. Google key from environment
4. Missing key returns error
5. Unknown provider returns error

## VCR (Video Cassette Recorder) Pattern

Tests use VCR infrastructure for HTTP recording/replay:
- `vcr_next_chunk()` - Retrieves next HTTP response chunk from fixture
- `vcr_has_more()` - Checks if more chunks available in playback
- `vcr_record_chunk()` - Writes response chunk to fixture (record mode)
- `vcr_record_response()` - Writes response metadata to fixture

VCR hooks into MOCKABLE curl_multi wrappers to intercept HTTP traffic:
- Playback mode (default): Reads JSONL fixtures, delivers chunks to curl write callbacks
- Record mode (VCR_RECORD=1): Captures real API responses, writes to JSONL fixtures

The VCR layer simulates the fdset/perform/info_read cycle so tests can verify the async pattern works correctly without real network I/O.

References:
- `scratch/plan/vcr-cassettes.md` - Complete VCR design specification
- `scratch/tasks/vcr-core.md` - VCR core infrastructure
- `scratch/tasks/vcr-mock-integration.md` - Curl integration layer

## Postconditions

- [ ] `tests/unit/providers/test_provider_factory.c` created with 11 tests (including async vtable tests)
- [ ] `tests/unit/providers/common/test_error_handling.c` created with 4 tests
- [ ] `tests/unit/test_credentials.c` created with 5 tests
- [ ] All tests verify async vtable methods (fdset/perform/timeout/info_read/start_request/start_stream)
- [ ] NO tests use blocking send()/stream() pattern (these do not exist in the async vtable)
- [ ] All tests use environment variables (no hardcoded keys)
- [ ] Tests clean up environment after each test
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-provider-core.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).