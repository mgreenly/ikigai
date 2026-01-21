#include "../../../test_constants.h"
/**
 * @file test_google_streaming_content.c
 * @brief Unit tests for Google streaming content events
 *
 * Tests content accumulation, thinking deltas, and tool call streaming
 * using VCR fixtures in JSONL format.
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
#include "../../../helpers/vcr_helper.h"

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
 * Basic Streaming Tests
 * ================================================================ */

START_TEST(test_stream_start_event) {
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

    /* First event should be IK_STREAM_START */
    vcr_ck_assert(captured_count > 0);
    vcr_ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    vcr_ck_assert_ptr_nonnull(captured_events[0].data.start.model);

    vcr_finish();
}

END_TEST

START_TEST(test_text_delta_events) {
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

    /* Should have text delta events */
    bool found_text_delta = false;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TEXT_DELTA) {
            found_text_delta = true;
            vcr_ck_assert_ptr_nonnull(captured_events[i].data.delta.text);
        }
    }

    vcr_ck_assert(found_text_delta);

    vcr_finish();
}

END_TEST

START_TEST(test_stream_done_event) {
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

    /* Last event should be IK_STREAM_DONE */
    vcr_ck_assert(captured_count > 0);
    vcr_ck_assert_int_eq(captured_events[captured_count - 1].type, IK_STREAM_DONE);

    /* Should have usage info */
    vcr_ck_assert(captured_events[captured_count - 1].data.done.usage.total_tokens > 0);

    vcr_finish();
}

END_TEST

START_TEST(test_completion_callback_invoked) {
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
 * Content Accumulation Tests
 * ================================================================ */

START_TEST(test_multiple_text_deltas) {
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

    /* Count text delta events */
    size_t delta_count = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TEXT_DELTA) {
            delta_count++;
        }
    }

    /* Should have multiple deltas */
    vcr_ck_assert(delta_count > 1);

    vcr_finish();
}

END_TEST

START_TEST(test_delta_content_preserved) {
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

    /* Verify text deltas contain data */
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TEXT_DELTA) {
            vcr_ck_assert_ptr_nonnull(captured_events[i].data.delta.text);
            vcr_ck_assert(strlen(captured_events[i].data.delta.text) > 0);
        }
    }

    vcr_finish();
}

END_TEST

START_TEST(test_event_order_preserved) {
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

    /* Verify ordering: START -> deltas -> DONE */
    vcr_ck_assert(captured_count >= 2);
    vcr_ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    vcr_ck_assert_int_eq(captured_events[captured_count - 1].type, IK_STREAM_DONE);

    vcr_finish();
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_content_suite(void)
{
    Suite *s = suite_create("Google Streaming Content");

    TCase *tc_basic = tcase_create("Basic Streaming");
    tcase_set_timeout(tc_basic, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_stream_start_event);
    tcase_add_test(tc_basic, test_text_delta_events);
    tcase_add_test(tc_basic, test_stream_done_event);
    tcase_add_test(tc_basic, test_completion_callback_invoked);
    suite_add_tcase(s, tc_basic);

    TCase *tc_content = tcase_create("Content Accumulation");
    tcase_set_timeout(tc_content, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_multiple_text_deltas);
    tcase_add_test(tc_content, test_delta_content_preserved);
    tcase_add_test(tc_content, test_event_order_preserved);
    suite_add_tcase(s, tc_content);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_content_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/google/google_streaming_content_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
