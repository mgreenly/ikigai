# Task: Create HTTP Client and SSE Parser Tests

**Model:** sonnet/thinking
**Depends on:** http-client.md, sse-parser.md, tests-mock-infrastructure.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `src/providers/common/http_client.c` - HTTP client implementation
- `src/providers/common/sse_parser.c` - SSE parser implementation
- `tests/helpers/mock_http.h` - Mock infrastructure

**Plan:**
- `scratch/plan/testing-strategy.md` - Test patterns

## Objective

Create comprehensive unit tests for the HTTP client wrapper and SSE parser. These are foundational utilities used by all providers.

## Interface

**Files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/common/test_http_client.c` | HTTP client tests |
| `tests/unit/providers/common/test_sse_parser.c` | SSE parser tests |

## Behaviors

**HTTP Client Tests (`test_http_client.c`):**

Test the `ik_http_client_t` API:

```c
// Setup/teardown using mock infrastructure
static void setup(void) {
    mock_http_reset();
}

START_TEST(test_http_client_successful_request)
{
    mock_http_response_t resp = {.status_code = 200, .body = "{}"};
    mock_http_set_response(&resp);

    ik_http_client_t *client = ik_http_client_create(test_ctx);
    res_t result = ik_http_client_post(client, "https://api.example.com",
                                        "{}", headers);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(client->status_code, 200);
}
END_TEST

START_TEST(test_http_client_network_error)
{
    mock_http_set_error(CURLE_COULDNT_CONNECT);

    ik_http_client_t *client = ik_http_client_create(test_ctx);
    res_t result = ik_http_client_post(client, "https://api.example.com",
                                        "{}", headers);
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_IO);
}
END_TEST

START_TEST(test_http_client_timeout)
{
    mock_http_set_error(CURLE_OPERATION_TIMEDOUT);

    ik_http_client_t *client = ik_http_client_create(test_ctx);
    res_t result = ik_http_client_post(client, "https://api.example.com",
                                        "{}", headers);
    ck_assert(is_err(&result));
    // Verify timeout handled appropriately
}
END_TEST

START_TEST(test_http_client_sets_headers)
{
    // Verify Authorization, Content-Type headers are set
}
END_TEST
```

**SSE Parser Tests (`test_sse_parser.c`):**

Test the `ik_sse_parser_t` API:

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

**HTTP Client (7 tests):**
1. Successful POST returns 200 and body
2. Network error returns ERR_IO
3. Timeout returns appropriate error
4. Headers are set correctly (Authorization, Content-Type)
5. Streaming mode invokes callback
6. Response body accumulated correctly
7. Client cleanup frees resources

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

- [ ] `tests/unit/providers/common/test_http_client.c` created with 7+ tests
- [ ] `tests/unit/providers/common/test_sse_parser.c` created with 9+ tests
- [ ] All tests use mock infrastructure (no real network calls)
- [ ] Tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
