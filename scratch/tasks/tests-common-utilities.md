# Task: Create HTTP Multi-Handle and SSE Parser Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** http-client.md, sse-parser.md, tests-mock-infrastructure.md

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

Reference: `src/openai/client_multi.c` (existing async implementation)

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] HTTP multi-handle implementation exists (`src/providers/common/http_multi.c`)
- [ ] Mock curl_multi infrastructure exists (`tests/helpers/mock_curl_multi.h`)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `src/providers/common/http_multi.c` - Async HTTP multi-handle implementation
- `src/providers/common/sse_parser.c` - SSE parser implementation
- `tests/helpers/mock_curl_multi.h` - Mock curl_multi infrastructure

**Plan:**
- `scratch/plan/testing-strategy.md` - Async test patterns with mock curl_multi

## Objective

Create comprehensive unit tests for the HTTP multi-handle client wrapper and SSE parser. These are foundational async utilities used by all providers. Tests must verify the async behavior through the fdset/perform/info_read pattern.

## Interface

**Files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/common/test_http_multi.c` | HTTP multi-handle tests |
| `tests/unit/providers/common/test_sse_parser.c` | SSE parser tests |

## Behaviors

**HTTP Multi-Handle Tests (`test_http_multi.c`):**

Test the async `ik_http_multi_t` API using mock curl_multi functions:

```c
// Setup/teardown using mock curl_multi infrastructure
static ik_http_multi_t *multi;
static ik_response_t *captured_response;
static res_t captured_result;

static void test_completion_cb(const ik_http_completion_t *completion, void *ctx) {
    captured_result = completion->result;
    captured_response = completion->response;
}

static void setup(void) {
    mock_curl_multi_reset();
    captured_response = NULL;
    multi = NULL;
    res_t r = ik_http_multi_create(test_ctx, &multi);
    ck_assert(is_ok(&r));
}

START_TEST(test_http_multi_fdset_integration)
{
    // Verify fdset() populates fd_sets from curl_multi_fdset
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    mock_curl_multi_set_fdset(3, 4, -1);  // Mock: read=3, write=4, exc=none

    res_t r = ik_http_multi_fdset(multi, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(is_ok(&r));
    ck_assert(FD_ISSET(3, &read_fds));
    ck_assert(FD_ISSET(4, &write_fds));
    ck_assert_int_eq(max_fd, 4);
}
END_TEST

START_TEST(test_http_multi_perform_non_blocking)
{
    // Verify perform() drives curl_multi_perform
    mock_curl_multi_set_running_handles(1);

    int running = 0;
    res_t r = ik_http_multi_perform(multi, &running);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(running, 1);
}
END_TEST

START_TEST(test_http_multi_add_request_returns_immediately)
{
    // Verify start_request() returns immediately (non-blocking)
    ik_request_t req = {.url = "https://api.example.com", .body = "{}"};
    mock_curl_multi_set_running_handles(1);

    res_t r = ik_http_multi_add_request(multi, &req, test_completion_cb, NULL);
    ck_assert(is_ok(&r));
    // Callback NOT invoked yet - request just queued
    ck_assert_ptr_null(captured_response);
}
END_TEST

START_TEST(test_http_multi_completion_via_info_read)
{
    // Verify completion callback invoked from info_read()
    ik_request_t req = {.url = "https://api.example.com", .body = "{}"};

    // Start request
    ik_http_multi_add_request(multi, &req, test_completion_cb, NULL);

    // Mock successful completion
    mock_curl_multi_set_completion(CURLE_OK, 200, "{}");

    // Drive event loop
    int running = 0;
    ik_http_multi_perform(multi, &running);
    ik_http_multi_info_read(multi, NULL);

    // Callback should now have been invoked
    ck_assert_ptr_nonnull(captured_response);
    ck_assert(is_ok(&captured_result));
}
END_TEST

START_TEST(test_http_multi_timeout_returns_curl_timeout)
{
    // Verify timeout() returns curl's recommended timeout
    mock_curl_multi_set_timeout(100);  // 100ms

    long timeout_ms = 0;
    res_t r = ik_http_multi_timeout(multi, &timeout_ms);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(timeout_ms, 100);
}
END_TEST

START_TEST(test_http_multi_network_error_via_callback)
{
    // Verify network errors delivered via completion callback
    ik_request_t req = {.url = "https://api.example.com", .body = "{}"};
    ik_http_multi_add_request(multi, &req, test_completion_cb, NULL);

    mock_curl_multi_set_completion(CURLE_COULDNT_CONNECT, 0, NULL);

    int running = 0;
    ik_http_multi_perform(multi, &running);
    ik_http_multi_info_read(multi, NULL);

    ck_assert(is_err(&captured_result));
    ck_assert_int_eq(captured_result.err->code, ERR_IO);
}
END_TEST

START_TEST(test_http_multi_event_loop_cycle)
{
    // Full event loop cycle test
    ik_request_t req = {.url = "https://api.example.com", .body = "{}"};
    ik_http_multi_add_request(multi, &req, test_completion_cb, NULL);

    // Simulate multiple perform() cycles before completion
    mock_curl_multi_set_running_handles(1);
    int running = 1;

    while (running > 0 && captured_response == NULL) {
        fd_set r, w, e;
        int max_fd = 0;
        FD_ZERO(&r); FD_ZERO(&w); FD_ZERO(&e);

        ik_http_multi_fdset(multi, &r, &w, &e, &max_fd);
        // select() would be called here in real code
        ik_http_multi_perform(multi, &running);
        ik_http_multi_info_read(multi, NULL);

        // Simulate completion on second iteration
        if (running > 0) {
            mock_curl_multi_set_completion(CURLE_OK, 200, "{\"result\":\"ok\"}");
            mock_curl_multi_set_running_handles(0);
        }
    }

    ck_assert_ptr_nonnull(captured_response);
}
END_TEST
```

**SSE Parser Tests (`test_sse_parser.c`):**

Test the `ik_sse_parser_t` API (synchronous, called from curl write callback):

```c
START_TEST(test_sse_parser_complete_event)
{
    ik_sse_parser_t *parser = ik_sse_parser_create(test_ctx);

    const char *data = "event: message\ndata: {\"text\":\"hello\"}\n\n";
    ik_sse_event_t *event = NULL;
    res_t result = ik_sse_parser_feed(parser, data, strlen(data), &event);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(event->event, "message");
    ck_assert_str_eq(event->data, "{\"text\":\"hello\"}");
}
END_TEST

START_TEST(test_sse_parser_data_only)
{
    // Event with only data: field, no event: field
    const char *data = "data: simple\n\n";
    // ...
}
END_TEST

START_TEST(test_sse_parser_multiline_data)
{
    // Multiple data: lines concatenated
    const char *data = "data: line1\ndata: line2\n\n";
    // Result should be "line1\nline2"
}
END_TEST

START_TEST(test_sse_parser_done_marker)
{
    const char *data = "data: [DONE]\n\n";
    // Should signal stream end
}
END_TEST

START_TEST(test_sse_parser_ignores_comments)
{
    const char *data = ": this is a comment\ndata: real\n\n";
    // Should only parse the data line
}
END_TEST

START_TEST(test_sse_parser_handles_partial_chunks)
{
    // Feed data in multiple chunks, verify accumulation
    ik_sse_parser_feed(parser, "data: hel", 9, &event);
    ck_assert_ptr_null(event);  // Not complete yet

    ik_sse_parser_feed(parser, "lo\n\n", 4, &event);
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(event->data, "hello");
}
END_TEST

START_TEST(test_sse_parser_malformed_graceful)
{
    // Invalid format doesn't crash
    const char *data = "garbage without newlines";
    res_t result = ik_sse_parser_feed(parser, data, strlen(data), &event);
    // Should not crash, may return NULL event
}
END_TEST
```

## Test Scenarios

**HTTP Multi-Handle (8 tests):**
1. fdset() populates fd_sets from curl_multi_fdset
2. perform() drives curl_multi_perform (non-blocking)
3. start_request() returns immediately, doesn't invoke callback
4. Completion callback invoked from info_read() after transfer completes
5. timeout() returns curl's recommended timeout
6. Network errors delivered via completion callback, not return value
7. Full event loop cycle (fdset/perform/info_read)
8. Multi-handle cleanup frees resources

**SSE Parser (9 tests):**
1. Complete event with event: and data:
2. Event with only data: field
3. Multi-line data concatenation
4. [DONE] marker detection
5. Comment lines ignored
6. Partial chunk accumulation
7. Empty lines as event separator
8. Malformed input handled gracefully
9. Parser reset between events

## Postconditions

- [ ] `tests/unit/providers/common/test_http_multi.c` created with 8+ tests
- [ ] `tests/unit/providers/common/test_sse_parser.c` created with 9+ tests
- [ ] All tests use mock curl_multi infrastructure (no real network calls)
- [ ] Tests verify async behavior through fdset/perform/info_read pattern
- [ ] Tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-common-utilities.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
