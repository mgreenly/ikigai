/**
 * @file google_vtable_test.c
 * @brief Unit tests for Google provider vtable methods (start_request)
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include <string.h>
#include "providers/google/google.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "logger.h"
#include "../../../helpers/vcr.h"

static TALLOC_CTX *test_ctx;
static ik_provider_t *provider;
static ik_request_t *request;

/* Test completion capture */
static ik_provider_completion_t test_completion;
static bool test_completion_called;

/* ================================================================
 * Callbacks
 * ================================================================ */

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)ctx;
    test_completion = *completion;
    test_completion_called = true;
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
    request->model = talloc_strdup(request, "gemini-2.0-flash");
    request->max_output_tokens = 1024;

    /* Add user message */
    request->messages = talloc_zero_array(request, ik_message_t, 1);
    request->message_count = 1;
    request->messages[0].role = IK_ROLE_USER;
    request->messages[0].content_blocks = talloc_zero_array(request, ik_content_block_t, 1);
    request->messages[0].content_count = 1;
    request->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    request->messages[0].content_blocks[0].data.text.text = talloc_strdup(request, "Hello!");

    /* Reset completion capture */
    test_completion_called = false;
    memset(&test_completion, 0, sizeof(test_completion));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * start_request Tests
 * ================================================================ */

START_TEST(test_start_request_returns_immediately) {
    vcr_init("request_basic", "google");

    /* start_request should return immediately without blocking */
    res_t r = provider->vt->start_request(provider->ctx, request,
                                         test_completion_cb, NULL);

    vcr_ck_assert(!is_err(&r));

    /* Should not have called completion yet */
    vcr_ck_assert(!test_completion_called);

    vcr_finish();
}
END_TEST

START_TEST(test_start_request_event_loop) {
    vcr_init("request_basic", "google");

    /* Start request */
    res_t r = provider->vt->start_request(provider->ctx, request,
                                         test_completion_cb, NULL);
    vcr_ck_assert(!is_err(&r));

    /* Run event loop */
    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    int running_handles = 1;
    int iterations = 0;
    const int max_iterations = 100;

    while (running_handles > 0 && iterations < max_iterations) {
        fd_set read_fds, write_fds, exc_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&exc_fds);
        int max_fd = 0;

        r = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
        vcr_ck_assert(!is_err(&r));

        long timeout_ms = 0;
        r = provider->vt->timeout(provider->ctx, &timeout_ms);
        vcr_ck_assert(!is_err(&r));

        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        select(max_fd + 1, &read_fds, &write_fds, &exc_fds, &timeout);

        r = provider->vt->perform(provider->ctx, &running_handles);
        vcr_ck_assert(!is_err(&r));

        provider->vt->info_read(provider->ctx, logger);

        iterations++;
    }

    /* Should have completed */
    vcr_ck_assert(test_completion_called);
    vcr_ck_assert(test_completion.success);

    vcr_finish();
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_vtable_suite(void)
{
    Suite *s = suite_create("Google Vtable");

    TCase *tc_request = tcase_create("start_request");
    tcase_add_unchecked_fixture(tc_request, setup, teardown);
    tcase_add_test(tc_request, test_start_request_returns_immediately);
    tcase_add_test(tc_request, test_start_request_event_loop);
    suite_add_tcase(s, tc_request);

    return s;
}

int main(void)
{
    Suite *s = google_vtable_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
