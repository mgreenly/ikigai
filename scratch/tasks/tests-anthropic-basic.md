# Task: Create Anthropic Provider Basic Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** anthropic-core.md, anthropic-request.md, anthropic-response.md, tests-mock-infrastructure.md

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
    res_t r = ik_anthropic_create(ctx, "test-key", &provider);
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

## Interface

**Test files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/anthropic/test_anthropic_adapter.c` | Provider vtable implementation (async methods) |
| `tests/unit/providers/anthropic/test_anthropic_client.c` | Request serialization to Messages API |
| `tests/unit/providers/anthropic/test_anthropic_errors.c` | Error response parsing and mapping |

**Fixture files to create:**

| File | Purpose |
|------|---------|
| `tests/fixtures/anthropic/response_basic.json` | Standard completion response |
| `tests/fixtures/anthropic/response_thinking.json` | Response with thinking content block |
| `tests/fixtures/anthropic/response_tool_call.json` | Response with tool use content block |
| `tests/fixtures/anthropic/error_auth.json` | 401 authentication error |
| `tests/fixtures/anthropic/error_rate_limit.json` | 429 rate limit error |

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
- [ ] 5 fixture files created with valid JSON
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