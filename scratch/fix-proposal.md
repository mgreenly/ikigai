# Implementation Proposal: openai_start_stream()

## Overview

Complete the vtable integration for OpenAI streaming by implementing the `openai_start_stream()` stub in `src/providers/openai/openai.c`.

## Reference Implementation

The existing `openai_start_request()` (lines 285-382 of `openai.c`) provides the complete pattern. We need to adapt it for streaming.

## Key Differences: Non-Streaming vs Streaming

| Aspect | openai_start_request() | openai_start_stream() |
|--------|------------------------|------------------------|
| JSON param | `stream=false` (default) | `stream=true` |
| Write callback | NULL (accumulate body) | SSE write callback |
| Write context | NULL | Stream context |
| Completion handler | `http_completion_handler` | `stream_completion_handler` |
| Request context | `ik_openai_request_ctx_t` | `ik_openai_stream_request_ctx_t` |

## Implementation Pseudocode

```c
static res_t openai_start_stream(void *ctx, const ik_request_t *req,
                                  ik_stream_cb_t stream_cb, void *stream_ctx,
                                  ik_provider_completion_cb_t completion_cb,
                                  void *completion_ctx)
{
    ik_openai_ctx_t *impl_ctx = (ik_openai_ctx_t *)ctx;

    // 1. Determine API mode (same as non-streaming)
    bool use_responses_api = impl_ctx->use_responses_api
        || ik_openai_prefer_responses_api(req->model);

    // 2. Create streaming request context
    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(impl_ctx, ...);
    req_ctx->provider = impl_ctx;
    req_ctx->use_responses_api = use_responses_api;
    req_ctx->stream_cb = stream_cb;
    req_ctx->stream_ctx = stream_ctx;
    req_ctx->completion_cb = completion_cb;
    req_ctx->completion_ctx = completion_ctx;

    // 3. Create streaming parser context (lives on req_ctx)
    ik_openai_chat_stream_ctx_t *parser_ctx =
        ik_openai_chat_stream_ctx_create(req_ctx, stream_cb, stream_ctx);
    req_ctx->parser_ctx = parser_ctx;

    // 4. Serialize request with stream=true
    char *json_body = NULL;
    res_t serialize_res;

    if (use_responses_api) {
        serialize_res = ik_openai_serialize_responses_request(req_ctx, req, true, &json_body);
    } else {
        serialize_res = ik_openai_serialize_chat_request(req_ctx, req, true, &json_body);
    }

    if (is_err(&serialize_res)) {
        talloc_steal(impl_ctx, serialize_res.err);
        talloc_free(req_ctx);
        return serialize_res;
    }

    // 5. Build URL (same as non-streaming)
    char *url = NULL;
    res_t url_res;

    if (use_responses_api) {
        url_res = ik_openai_build_responses_url(req_ctx, impl_ctx->base_url, &url);
    } else {
        url_res = ik_openai_build_chat_url(req_ctx, impl_ctx->base_url, &url);
    }

    if (is_err(&url_res)) {
        talloc_steal(impl_ctx, url_res.err);
        talloc_free(req_ctx);
        return url_res;
    }

    // 6. Build headers (same as non-streaming)
    char **headers_tmp = NULL;
    res_t headers_res = ik_openai_build_headers(req_ctx, impl_ctx->api_key, &headers_tmp);
    if (is_err(&headers_res)) {
        talloc_steal(impl_ctx, headers_res.err);
        talloc_free(req_ctx);
        return headers_res;
    }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-qual"
    const char **headers_const = (const char **)headers_tmp;
    #pragma GCC diagnostic pop

    // 7. Build HTTP request (with streaming write callback)
    ik_http_request_t http_req = {
        .url = url,
        .method = "POST",
        .headers = headers_const,
        .body = json_body,
        .body_len = strlen(json_body)
    };

    // 8. Add request with SSE write callback
    res_t add_res = ik_http_multi_add_request(
        impl_ctx->http_multi,
        &http_req,
        openai_stream_write_callback,     // NEW: SSE processing
        req_ctx,                           // NEW: Context with parser_ctx
        openai_stream_completion_handler,  // NEW: Streaming completion
        req_ctx);

    if (is_err(&add_res)) {
        talloc_steal(impl_ctx, add_res.err);
        talloc_free(req_ctx);
        return add_res;
    }

    // 9. Return immediately (async)
    return OK(NULL);
}
```

## New Types Required

### Stream Request Context

```c
/**
 * Internal request context for tracking in-flight streaming requests
 */
typedef struct {
    ik_openai_ctx_t *provider;              // Provider context
    bool use_responses_api;                  // Which API mode
    ik_stream_cb_t stream_cb;                // User's stream callback
    void *stream_ctx;                        // User's stream context
    ik_provider_completion_cb_t completion_cb;  // User's completion callback
    void *completion_ctx;                    // User's completion context
    ik_openai_chat_stream_ctx_t *parser_ctx; // SSE parser context
    // TODO: Add SSE line buffer for incomplete events
    char *sse_buffer;                        // Accumulate incomplete SSE lines
    size_t sse_buffer_len;                   // Current buffer length
} ik_openai_stream_request_ctx_t;
```

## New Callbacks Required

### SSE Write Callback

```c
/**
 * curl write callback for SSE streaming
 *
 * Called during perform() as HTTP chunks arrive.
 * Parses SSE format and feeds data events to parser.
 */
static size_t openai_stream_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    ik_openai_stream_request_ctx_t *req_ctx = (ik_openai_stream_request_ctx_t *)userdata;
    size_t total_size = size * nmemb;
    const char *data = (const char *)ptr;

    // Parse SSE format:
    // - Lines are terminated by \n
    // - "data: <json>" lines contain event data
    // - Process each complete "data:" line

    // Append to buffer (handles incomplete lines across chunks)
    char *new_buffer = talloc_realloc(req_ctx, req_ctx->sse_buffer,
                                       char, req_ctx->sse_buffer_len + total_size + 1);
    if (new_buffer == NULL) PANIC("Out of memory");

    memcpy(new_buffer + req_ctx->sse_buffer_len, data, total_size);
    req_ctx->sse_buffer_len += total_size;
    new_buffer[req_ctx->sse_buffer_len] = '\0';
    req_ctx->sse_buffer = new_buffer;

    // Process complete lines
    char *line_start = req_ctx->sse_buffer;
    char *line_end;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';  // Null-terminate line

        // Check for "data: " prefix
        if (strncmp(line_start, "data: ", 6) == 0) {
            const char *json_data = line_start + 6;

            // Feed to parser - this invokes user's stream_cb
            ik_openai_chat_stream_process_data(req_ctx->parser_ctx, json_data);
        }

        // Move to next line
        line_start = line_end + 1;
    }

    // Keep incomplete line in buffer
    size_t remaining = req_ctx->sse_buffer_len - (line_start - req_ctx->sse_buffer);
    if (remaining > 0) {
        memmove(req_ctx->sse_buffer, line_start, remaining);
        req_ctx->sse_buffer_len = remaining;
        req_ctx->sse_buffer[remaining] = '\0';
    } else {
        talloc_free(req_ctx->sse_buffer);
        req_ctx->sse_buffer = NULL;
        req_ctx->sse_buffer_len = 0;
    }

    return total_size;  // Tell curl we consumed all bytes
}
```

### Stream Completion Callback

```c
/**
 * HTTP completion callback for streaming requests
 *
 * Called from info_read() when HTTP transfer completes.
 * Invokes user's completion callback with final metadata.
 */
static void openai_stream_completion_handler(const ik_http_completion_t *http_completion,
                                               void *user_ctx)
{
    ik_openai_stream_request_ctx_t *req_ctx = (ik_openai_stream_request_ctx_t *)user_ctx;
    assert(req_ctx != NULL);
    assert(req_ctx->completion_cb != NULL);

    // Build provider completion structure
    ik_provider_completion_t provider_completion = {0};
    provider_completion.http_status = http_completion->http_code;

    // Handle HTTP errors (same pattern as non-streaming)
    if (http_completion->type != IK_HTTP_SUCCESS) {
        provider_completion.success = false;
        provider_completion.response = NULL;
        provider_completion.retry_after_ms = -1;

        // Parse error if JSON body available
        if (http_completion->response_body != NULL && http_completion->response_len > 0) {
            ik_error_category_t category;
            char *error_msg = NULL;
            res_t parse_res = ik_openai_parse_error(req_ctx, http_completion->http_code,
                                                     http_completion->response_body,
                                                     http_completion->response_len,
                                                     &category, &error_msg);
            if (is_ok(&parse_res)) {
                provider_completion.error_category = category;
                provider_completion.error_message = error_msg;
            } else {
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
                provider_completion.error_message = talloc_asprintf(req_ctx,
                    "HTTP %d error", http_completion->http_code);
            }
        } else {
            if (http_completion->http_code == 0) {
                provider_completion.error_category = IK_ERR_CAT_NETWORK;
            } else {
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
            }
            provider_completion.error_message = http_completion->error_message != NULL
                ? talloc_strdup(req_ctx, http_completion->error_message)
                : talloc_asprintf(req_ctx, "HTTP %d error", http_completion->http_code);
        }

        req_ctx->completion_cb(&provider_completion, req_ctx->completion_ctx);
        talloc_free(req_ctx);
        return;
    }

    // Success - build completion with metadata from stream
    provider_completion.success = true;
    provider_completion.response = NULL;  // Streaming - no accumulated response
    provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
    provider_completion.error_message = NULL;
    provider_completion.retry_after_ms = -1;

    // Note: Stream events were already delivered during perform()
    // This completion just signals "HTTP transfer done"
    // Metadata (usage, finish_reason) were included in final STREAM_DONE event

    req_ctx->completion_cb(&provider_completion, req_ctx->completion_ctx);

    // Cleanup
    talloc_free(req_ctx);
}
```

## File Changes Summary

### Modified Files

**src/providers/openai/openai.c:**
- Add `ik_openai_stream_request_ctx_t` struct definition
- Add `openai_stream_write_callback()` function
- Add `openai_stream_completion_handler()` function
- Replace `openai_start_stream()` stub with full implementation

Lines to add: ~150
Lines to remove: ~20 (stub)

### No New Files Required

All code goes in existing `openai.c` file. The SSE parser (`streaming_chat.c`) already exists and works correctly.

## Testing Strategy

### Phase 1: Compile Test
```bash
make clean && make all
# Should compile without errors
```

### Phase 2: Existing Parser Tests
```bash
make build/tests/unit/providers/openai/openai_streaming_test
./build/tests/unit/providers/openai/openai_streaming_test
# All 18 existing tests should still pass
```

### Phase 3: Add 3 Async Integration Tests

**Add to `tests/unit/providers/openai/openai_streaming_test.c`:**

```c
/* ================================================================
 * Async Vtable Integration Tests
 * ================================================================ */

START_TEST(test_start_stream_returns_immediately)
{
    // Create mock provider
    ik_provider_t *provider = NULL;
    res_t create_res = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&create_res));

    // Create mock request
    ik_request_t req = {
        .model = "gpt-4",
        .messages = NULL,
        .message_count = 0,
        .stream = true
    };

    // Start stream
    res_t start_res = provider->vt->start_stream(
        provider->ctx, &req, stream_cb, events,
        completion_cb, NULL);

    // Should return immediately without blocking
    ck_assert(is_ok(&start_res));
}
END_TEST

START_TEST(test_fdset_returns_valid_fds)
{
    ik_provider_t *provider = NULL;
    res_t create_res = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&create_res));

    // Call fdset
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    res_t fdset_res = provider->vt->fdset(provider->ctx,
        &read_fds, &write_fds, &exc_fds, &max_fd);

    // Should return OK
    ck_assert(is_ok(&fdset_res));
}
END_TEST

START_TEST(test_perform_info_read_completes_stream)
{
    ik_provider_t *provider = NULL;
    res_t create_res = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&create_res));

    // Mock: Set up fake SSE response in http_multi layer
    // (Requires mock infrastructure from VCR)

    ik_request_t req = {
        .model = "gpt-4",
        .messages = NULL,
        .message_count = 0,
        .stream = true
    };

    // Start stream
    provider->vt->start_stream(provider->ctx, &req, stream_cb, events,
                                completion_cb, NULL);

    // Simulate event loop
    int running = 1;
    while (running > 0) {
        provider->vt->perform(provider->ctx, &running);
    }

    // Read completion
    provider->vt->info_read(provider->ctx, NULL);

    // Verify stream events were emitted
    ck_assert_int_gt(events->count, 0);
}
END_TEST
```

Add to suite:
```c
/* Async Integration */
TCase *tc_async = tcase_create("AsyncIntegration");
tcase_add_checked_fixture(tc_async, setup, teardown);
tcase_add_test(tc_async, test_start_stream_returns_immediately);
tcase_add_test(tc_async, test_fdset_returns_valid_fds);
tcase_add_test(tc_async, test_perform_info_read_completes_stream);
suite_add_tcase(s, tc_async);
```

### Phase 4: Full Test Suite
```bash
make check
# All tests should pass (21 total in openai_streaming_test.c)
```

## Implementation Checklist

- [ ] 1. Define `ik_openai_stream_request_ctx_t` struct in `openai.c`
- [ ] 2. Implement `openai_stream_write_callback()` - SSE line parsing
- [ ] 3. Implement `openai_stream_completion_handler()` - metadata callback
- [ ] 4. Implement `openai_start_stream()` - vtable method (replace stub)
- [ ] 5. Compile: `make clean && make all`
- [ ] 6. Test parsers: `./build/tests/unit/providers/openai/openai_streaming_test`
- [ ] 7. Add 3 async integration tests
- [ ] 8. Full suite: `make check`
- [ ] 9. Commit: "fix: Complete openai_start_stream vtable integration"
- [ ] 10. Resume orchestration

## Estimated Effort

- **Code writing:** 150 lines (2 callbacks + struct + vtable method)
- **Testing:** 3 new tests (~60 lines)
- **Debug/iterate:** Unknown (depends on edge cases)
- **Total time:** 2-4 hours for experienced developer

## Risk Assessment

**Low risk:**
- ✓ Pattern exists (openai_start_request)
- ✓ Infrastructure exists (http_multi, parser)
- ✓ Parser tested (18 tests pass)
- ✓ No new external dependencies

**Medium complexity:**
- SSE line buffering (handle incomplete lines across chunks)
- Error handling edge cases
- Memory management (talloc ownership)

**High confidence:**
- Clear specification
- Working reference implementation
- Testable in isolation
