#include "tests/test_constants.h"
/**
 * @file google_streaming_advanced_test.c
 * @brief Unit tests for Google streaming thinking and tool call events
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include <string.h>
#include "apps/ikigai/providers/google/google.h"
#include "apps/ikigai/providers/google/streaming.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"
#include "shared/logger.h"
#include "tests/helpers/vcr_helper.h"

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
 * Thinking Content Tests
 * ================================================================ */

START_TEST(test_thinking_delta_event_type) {
    vcr_init("stream_thinking", "google");

    /* Configure thinking request */
    request->thinking.level = IK_THINKING_HIGH;

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

    /* Should have thinking delta events */
    bool found_thinking = false;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_THINKING_DELTA) {
            found_thinking = true;
            break;
        }
    }

    vcr_ck_assert(found_thinking);

    vcr_finish();
}

END_TEST

START_TEST(test_thinking_delta_content) {
    vcr_init("stream_thinking", "google");

    request->thinking.level = IK_THINKING_HIGH;

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

    /* Verify thinking content */
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_THINKING_DELTA) {
            vcr_ck_assert_ptr_nonnull(captured_events[i].data.delta.text);
            vcr_ck_assert(strlen(captured_events[i].data.delta.text) > 0);
        }
    }

    vcr_finish();
}

END_TEST

START_TEST(test_usage_includes_thinking_tokens) {
    vcr_init("stream_thinking", "google");

    request->thinking.level = IK_THINKING_HIGH;

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

    /* Check usage in DONE event */
    vcr_ck_assert(captured_count > 0);
    ik_stream_event_t *done_event = &captured_events[captured_count - 1];
    vcr_ck_assert_int_eq(done_event->type, IK_STREAM_DONE);
    vcr_ck_assert(done_event->data.done.usage.thinking_tokens > 0);

    vcr_finish();
}

END_TEST
/* ================================================================
 * Tool Call Streaming Tests
 * ================================================================ */

START_TEST(test_tool_call_start_event) {
    vcr_init("stream_tool_call", "google");

    /* Add tool definition */
    request->tools = talloc_zero_array(request, ik_tool_def_t, 1);
    request->tool_count = 1;
    request->tools[0].name = talloc_strdup(request, "get_weather");
    request->tools[0].description = talloc_strdup(request, "Get weather");
    request->tools[0].parameters = talloc_strdup(request, "{}");

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

    /* Should have tool call start event */
    bool found_tool_start = false;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TOOL_CALL_START) {
            found_tool_start = true;
            vcr_ck_assert_ptr_nonnull(captured_events[i].data.tool_start.id);
            vcr_ck_assert_ptr_nonnull(captured_events[i].data.tool_start.name);
            break;
        }
    }

    vcr_ck_assert(found_tool_start);

    vcr_finish();
}

END_TEST

START_TEST(test_tool_call_delta_events) {
    vcr_init("stream_tool_call", "google");

    request->tools = talloc_zero_array(request, ik_tool_def_t, 1);
    request->tool_count = 1;
    request->tools[0].name = talloc_strdup(request, "get_weather");
    request->tools[0].description = talloc_strdup(request, "Get weather");
    request->tools[0].parameters = talloc_strdup(request, "{}");

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

    /* Should have tool call delta events */
    bool found_tool_delta = false;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TOOL_CALL_DELTA) {
            found_tool_delta = true;
            vcr_ck_assert_ptr_nonnull(captured_events[i].data.tool_delta.arguments);
            break;
        }
    }

    vcr_ck_assert(found_tool_delta);

    vcr_finish();
}

END_TEST

START_TEST(test_tool_call_done_event) {
    vcr_init("stream_tool_call", "google");

    request->tools = talloc_zero_array(request, ik_tool_def_t, 1);
    request->tool_count = 1;
    request->tools[0].name = talloc_strdup(request, "get_weather");
    request->tools[0].description = talloc_strdup(request, "Get weather");
    request->tools[0].parameters = talloc_strdup(request, "{}");

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

    /* Should have tool call done event */
    bool found_tool_done = false;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TOOL_CALL_DONE) {
            found_tool_done = true;
            break;
        }
    }

    vcr_ck_assert(found_tool_done);

    vcr_finish();
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_advanced_suite(void)
{
    Suite *s = suite_create("Google Streaming Advanced");

    TCase *tc_thinking = tcase_create("Thinking Content");
    tcase_set_timeout(tc_thinking, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_thinking, setup, teardown);
    tcase_add_test(tc_thinking, test_thinking_delta_event_type);
    tcase_add_test(tc_thinking, test_thinking_delta_content);
    tcase_add_test(tc_thinking, test_usage_includes_thinking_tokens);
    suite_add_tcase(s, tc_thinking);

    TCase *tc_tools = tcase_create("Tool Call Streaming");
    tcase_set_timeout(tc_tools, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_tool_call_start_event);
    tcase_add_test(tc_tools, test_tool_call_delta_events);
    tcase_add_test(tc_tools, test_tool_call_done_event);
    suite_add_tcase(s, tc_tools);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_advanced_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/google_streaming_advanced_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
