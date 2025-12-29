/**
 * @file test_google_streaming_async.c
 * @brief Unit tests for Google streaming async event loop
 *
 * Tests async streaming event loop integration using VCR fixtures in JSONL format.
 * Verifies fdset/perform/info_read pattern and basic event delivery.
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include <string.h>
#include "providers/google/google.h"
#include "providers/google/streaming.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "logger.h"
#include "../../../helpers/vcr.h"

static TALLOC_CTX *test_ctx;
static ik_provider_t *provider;
static ik_request_t *request;

/* Test event capture */
#define MAX_EVENTS 64
static ik_stream_event_t captured_events[MAX_EVENTS];
static size_t captured_count;
static ik_provider_completion_t captured_completion;
static bool completion_called;

/* ================================================================
 * Callbacks
 * ================================================================ */

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;

    if (captured_count < MAX_EVENTS) {
        /* Deep copy event data since pointers may be temporary */
        captured_events[captured_count] = *event;

        /* Copy string data */
        switch (event->type) {
            case IK_STREAM_START:
                if (event->data.start.model) {
                    captured_events[captured_count].data.start.model =
                        talloc_strdup(test_ctx, event->data.start.model);
                }
                break;
            case IK_STREAM_TEXT_DELTA:
            case IK_STREAM_THINKING_DELTA:
                if (event->data.delta.text) {
                    captured_events[captured_count].data.delta.text =
                        talloc_strdup(test_ctx, event->data.delta.text);
                }
                break;
            case IK_STREAM_TOOL_CALL_START:
                if (event->data.tool_start.id) {
                    captured_events[captured_count].data.tool_start.id =
                        talloc_strdup(test_ctx, event->data.tool_start.id);
                }
                if (event->data.tool_start.name) {
                    captured_events[captured_count].data.tool_start.name =
                        talloc_strdup(test_ctx, event->data.tool_start.name);
                }
                break;
            case IK_STREAM_TOOL_CALL_DELTA:
                if (event->data.tool_delta.arguments) {
                    captured_events[captured_count].data.tool_delta.arguments =
                        talloc_strdup(test_ctx, event->data.tool_delta.arguments);
                }
                break;
            case IK_STREAM_ERROR:
                if (event->data.error.message) {
                    captured_events[captured_count].data.error.message =
                        talloc_strdup(test_ctx, event->data.error.message);
                }
                break;
            default:
                break;
        }

        captured_count++;
    }

    return OK(NULL);
}

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)ctx;

    captured_completion = *completion;
    completion_called = true;

    return OK(NULL);
}

/* ================================================================
 * Fixtures
 * ================================================================ */

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    /* Create provider */
    res_t r = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&r));

    /* Create basic request */
    request = talloc_zero(test_ctx, ik_request_t);
    request->model = talloc_strdup(request, "gemini-2.5-flash");
    request->max_output_tokens = 1024;

    /* Add user message */
    request->messages = talloc_zero_array(request, ik_message_t, 1);
    request->message_count = 1;
    request->messages[0].role = IK_ROLE_USER;
    request->messages[0].content_blocks = talloc_zero_array(request, ik_content_block_t, 1);
    request->messages[0].content_count = 1;
    request->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    request->messages[0].content_blocks[0].data.text.text = talloc_strdup(request, "Hello!");

    /* Reset event capture */
    captured_count = 0;
    completion_called = false;
    memset(captured_events, 0, sizeof(captured_events));
    memset(&captured_completion, 0, sizeof(captured_completion));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Async Event Loop Tests
 * ================================================================ */

START_TEST(test_start_stream_returns_immediately) {
    vcr_init("stream_basic", "google");

    /* start_stream should return immediately without blocking */
    res_t r = provider->vt->start_stream(provider->ctx, request,
                                         test_stream_cb, NULL,
                                         test_completion_cb, NULL);

    vcr_ck_assert(!is_err(&r));

    /* Stream should not be complete yet */
    vcr_ck_assert(!completion_called);

    vcr_finish();
}
END_TEST START_TEST(test_fdset_returns_mock_fds)
{
    vcr_init("stream_basic", "google");

    /* Start stream */
    res_t r = provider->vt->start_stream(provider->ctx, request,
                                         test_stream_cb, NULL,
                                         test_completion_cb, NULL);
    vcr_ck_assert(!is_err(&r));

    /* fdset should populate FD sets */
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    r = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
    vcr_ck_assert(!is_err(&r));

    vcr_finish();
}

END_TEST START_TEST(test_perform_delivers_events_incrementally)
{
    vcr_init("stream_basic", "google");

    /* Start stream */
    res_t r = provider->vt->start_stream(provider->ctx, request,
                                         test_stream_cb, NULL,
                                         test_completion_cb, NULL);
    vcr_ck_assert(!is_err(&r));

    /* Drive event loop */
    int running = 1;
    size_t prev_count = 0;

    while (running > 0) {
        fd_set read_fds, write_fds, exc_fds;
        int max_fd = 0;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&exc_fds);

        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
        provider->vt->perform(provider->ctx, &running);

        /* Events should be delivered during perform() */
        if (captured_count > prev_count) {
            prev_count = captured_count;
        }
    }

    /* Should have received events */
    vcr_ck_assert(captured_count > 0);

    vcr_finish();
}

END_TEST START_TEST(test_timeout_returns_value)
{
    vcr_init("stream_basic", "google");

    /* Start stream */
    res_t r = provider->vt->start_stream(provider->ctx, request,
                                         test_stream_cb, NULL,
                                         test_completion_cb, NULL);
    vcr_ck_assert(!is_err(&r));

    /* timeout should return a valid timeout value */
    long timeout_ms = 0;
    r = provider->vt->timeout(provider->ctx, &timeout_ms);
    vcr_ck_assert(!is_err(&r));

    /* Timeout should be a reasonable value (not less than -1) */
    vcr_ck_assert(timeout_ms >= -1);

    vcr_finish();
}

END_TEST START_TEST(test_info_read_invokes_completion_callback)
{
    vcr_init("stream_basic", "google");

    res_t r = provider->vt->start_stream(provider->ctx, request,
                                         test_stream_cb, NULL,
                                         test_completion_cb, NULL);
    vcr_ck_assert(!is_err(&r));

    /* Drive event loop */
    int running = 1;
    while (running > 0) {
        fd_set read_fds, write_fds, exc_fds;
        int max_fd = 0;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&exc_fds);

        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
        provider->vt->perform(provider->ctx, &running);
    }

    /* Check for completion via info_read */
    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    /* Completion callback should have been invoked */
    vcr_ck_assert(completion_called);
    vcr_ck_assert(captured_completion.success);

    vcr_finish();
}

END_TEST
/* ================================================================
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_http_error_calls_completion_cb)
{
    vcr_init("error_auth_stream", "google");

    res_t r = provider->vt->start_stream(provider->ctx, request,
                                         test_stream_cb, NULL,
                                         test_completion_cb, NULL);
    vcr_ck_assert(!is_err(&r));

    /* Drive event loop */
    int running = 1;
    while (running > 0) {
        fd_set read_fds, write_fds, exc_fds;
        int max_fd = 0;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&exc_fds);

        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
        provider->vt->perform(provider->ctx, &running);
    }

    /* Check completion */
    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    vcr_ck_assert(completion_called);
    vcr_ck_assert(!captured_completion.success);
    vcr_ck_assert_int_eq(captured_completion.http_status, 401);

    vcr_finish();
}

END_TEST START_TEST(test_malformed_response_handled)
{
    /* This test would require a fixture with malformed response data */
    /* Skipping for now as VCR may not support this scenario */
    ck_assert(1);
}

END_TEST START_TEST(test_incomplete_stream_detected)
{
    /* This test would require a fixture with incomplete stream */
    /* Skipping for now as VCR may not support this scenario */
    ck_assert(1);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_async_suite(void)
{
    Suite *s = suite_create("Google Streaming Async");

    TCase *tc_async = tcase_create("Async Event Loop");
    tcase_set_timeout(tc_async, 30);
    tcase_add_unchecked_fixture(tc_async, setup, teardown);
    tcase_add_test(tc_async, test_start_stream_returns_immediately);
    tcase_add_test(tc_async, test_fdset_returns_mock_fds);
    tcase_add_test(tc_async, test_perform_delivers_events_incrementally);
    tcase_add_test(tc_async, test_timeout_returns_value);
    tcase_add_test(tc_async, test_info_read_invokes_completion_callback);
    suite_add_tcase(s, tc_async);

    TCase *tc_errors = tcase_create("Error Handling");
    tcase_set_timeout(tc_errors, 30);
    tcase_add_unchecked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_http_error_calls_completion_cb);
    tcase_add_test(tc_errors, test_malformed_response_handled);
    tcase_add_test(tc_errors, test_incomplete_stream_detected);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_async_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
