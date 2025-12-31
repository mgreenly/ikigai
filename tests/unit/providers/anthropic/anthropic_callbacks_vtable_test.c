/**
 * @file anthropic_callbacks_vtable_test.c
 * @brief Coverage tests for anthropic.c vtable methods and lifecycle
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/anthropic.h"
#include "providers/anthropic/anthropic_internal.h"
#include "providers/common/http_multi.h"
#include "providers/common/sse_parser.h"
#include "wrapper.h"
#include "wrapper_internal.h"
#include "logger.h"
#include "error.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* Info Read Tests */

// We need to expose the internal context structure for testing
typedef struct {
    char *api_key;
    char *base_url;
    ik_http_multi_t *http_multi;
    ik_anthropic_active_stream_t *active_stream;
} ik_anthropic_ctx_t;

// Mock for ik_http_multi_create_
res_t ik_http_multi_create_(void *parent, void **out)
{
    // Success case - create a dummy http_multi
    void *http_multi = talloc_zero_(parent, 1);  // Minimal allocation
    *out = http_multi;
    return OK(http_multi);
}

// Mock for ik_http_multi_info_read_
void ik_http_multi_info_read_(void *http_multi, void *logger)
{
    (void)http_multi;
    (void)logger;
    // Do nothing - just a mock
}

// Completion callback tracker
static bool g_completion_called = false;
static ik_provider_completion_t g_last_completion;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)ctx;
    g_completion_called = true;
    g_last_completion = *completion;
    return OK(NULL);
}

START_TEST(test_info_read_no_active_stream)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    // Call info_read with no active stream
    provider->vt->info_read(provider->ctx, NULL);

    // Should not crash, just returns
}

END_TEST

START_TEST(test_info_read_success_http_status)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    // Create active stream with 299 status (edge of 2xx range)
    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = true;
    stream->http_status = 299;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    g_completion_called = false;
    provider->vt->info_read(provider->ctx, NULL);

    // Verify completion callback was invoked with success
    ck_assert(g_completion_called);
    ck_assert(g_last_completion.success);
    ck_assert_int_eq(g_last_completion.http_status, 299);
}

END_TEST

static void test_error_status(int http_status, ik_error_category_t expected_cat)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;
    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = true;
    stream->http_status = http_status;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    g_completion_called = false;
    provider->vt->info_read(provider->ctx, NULL);

    ck_assert(g_completion_called);
    ck_assert(!g_last_completion.success);
    ck_assert_int_eq(g_last_completion.error_category, expected_cat);
}

START_TEST(test_info_read_auth_error_401)
{
    test_error_status(401, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_info_read_auth_error_403)
{
    test_error_status(403, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_info_read_rate_limit_429)
{
    test_error_status(429, IK_ERR_CAT_RATE_LIMIT);
}
END_TEST

START_TEST(test_info_read_server_error_500)
{
    test_error_status(500, IK_ERR_CAT_SERVER);
}
END_TEST

START_TEST(test_info_read_unknown_error_400)
{
    test_error_status(400, IK_ERR_CAT_UNKNOWN);
}
END_TEST

START_TEST(test_info_read_informational_status_100)
{
    test_error_status(100, IK_ERR_CAT_UNKNOWN);
}
END_TEST

START_TEST(test_info_read_no_completion_callback)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = true;
    stream->http_status = 200;
    stream->completion_cb = NULL;  // No callback
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    // Should not crash even without callback
    provider->vt->info_read(provider->ctx, NULL);
}

END_TEST

START_TEST(test_info_read_stream_not_completed)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = false;  // Not completed yet
    stream->http_status = 0;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    g_completion_called = false;

    // Call info_read - should not invoke callback since stream not completed
    provider->vt->info_read(provider->ctx, NULL);

    ck_assert(!g_completion_called);  // Callback should NOT be called
    ck_assert(impl_ctx->active_stream != NULL);  // Stream should still be active
}

END_TEST

/* Cancel Tests */

START_TEST(test_cancel_with_active_stream)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    // Get internal context
    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    // Create active stream
    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = false;
    stream->http_status = 0;
    impl_ctx->active_stream = stream;

    // Call cancel
    provider->vt->cancel(provider->ctx);

    // Verify stream is marked as completed
    ck_assert(impl_ctx->active_stream->completed);
}

END_TEST

START_TEST(test_cancel_without_active_stream)
{
    // Test cancel when no active stream exists
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    // Should not crash
    provider->vt->cancel(provider->ctx);
}

END_TEST

/* Vtable Method Tests - fdset, perform, timeout, cleanup */

START_TEST(test_anthropic_fdset)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    res_t result = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);

    // Just check it doesn't crash - result may be error if curl not initialized
    // The function is exercised which is what we need for coverage
    (void)result;
}

END_TEST

START_TEST(test_anthropic_perform)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    int running_handles = 0;

    res_t result = provider->vt->perform(provider->ctx, &running_handles);

    // Just check it doesn't crash - result may be error if curl not initialized
    // The function is exercised which is what we need for coverage
    (void)result;
}

END_TEST

START_TEST(test_anthropic_timeout)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    long timeout_ms = 0;

    res_t result = provider->vt->timeout(provider->ctx, &timeout_ms);

    // Just check it doesn't crash - result may be error if curl not initialized
    // The function is exercised which is what we need for coverage
    (void)result;
}

END_TEST

START_TEST(test_anthropic_cleanup)
{
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    // Should not crash - just a no-op function
    provider->vt->cleanup(provider->ctx);
}

END_TEST

/* start_stream Error Path Tests */

static res_t dummy_stream_cb_test(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

START_TEST(test_start_stream_null_model_error)
{
    // Test that start_stream properly handles serialization failure
    // when model is NULL (covers lines 305-307)
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    // Create a request with NULL model
    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_blocks = NULL,
        .content_count = 0,
        .provider_metadata = NULL
    };

    ik_request_t req = {
        .system_prompt = NULL,
        .messages = &msg,
        .message_count = 1,
        .model = NULL,  // NULL model triggers serialization error
        .thinking = {.level = IK_THINKING_NONE, .include_summary = false},
        .tools = NULL,
        .tool_count = 0,
        .max_output_tokens = 100,
        .tool_choice_mode = 0,
        .tool_choice_name = NULL
    };

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb_test, NULL,
                                    test_completion_cb, NULL);

    // Should return error from serialization
    ck_assert(is_err(&r));
    ck_assert_ptr_nonnull(r.err);
    ck_assert_str_eq(r.err->msg, "Model cannot be NULL");
}

END_TEST

/* Test Suite Setup */

static Suite *anthropic_callbacks_vtable_suite(void)
{
    Suite *s = suite_create("Anthropic Callbacks - Vtable");

    TCase *tc_info_read = tcase_create("Info Read");
    tcase_set_timeout(tc_info_read, 30);
    tcase_add_unchecked_fixture(tc_info_read, setup, teardown);
    tcase_add_test(tc_info_read, test_info_read_no_active_stream);
    tcase_add_test(tc_info_read, test_info_read_success_http_status);
    tcase_add_test(tc_info_read, test_info_read_auth_error_401);
    tcase_add_test(tc_info_read, test_info_read_auth_error_403);
    tcase_add_test(tc_info_read, test_info_read_rate_limit_429);
    tcase_add_test(tc_info_read, test_info_read_server_error_500);
    tcase_add_test(tc_info_read, test_info_read_unknown_error_400);
    tcase_add_test(tc_info_read, test_info_read_informational_status_100);
    tcase_add_test(tc_info_read, test_info_read_no_completion_callback);
    tcase_add_test(tc_info_read, test_info_read_stream_not_completed);
    suite_add_tcase(s, tc_info_read);

    TCase *tc_cancel = tcase_create("Cancel");
    tcase_set_timeout(tc_cancel, 30);
    tcase_add_unchecked_fixture(tc_cancel, setup, teardown);
    tcase_add_test(tc_cancel, test_cancel_with_active_stream);
    tcase_add_test(tc_cancel, test_cancel_without_active_stream);
    suite_add_tcase(s, tc_cancel);

    TCase *tc_vtable = tcase_create("Vtable Methods");
    tcase_set_timeout(tc_vtable, 30);
    tcase_add_unchecked_fixture(tc_vtable, setup, teardown);
    tcase_add_test(tc_vtable, test_anthropic_fdset);
    tcase_add_test(tc_vtable, test_anthropic_perform);
    tcase_add_test(tc_vtable, test_anthropic_timeout);
    tcase_add_test(tc_vtable, test_anthropic_cleanup);
    suite_add_tcase(s, tc_vtable);

    TCase *tc_start_stream_errors = tcase_create("start_stream Error Paths");
    tcase_set_timeout(tc_start_stream_errors, 30);
    tcase_add_unchecked_fixture(tc_start_stream_errors, setup, teardown);
    tcase_add_test(tc_start_stream_errors, test_start_stream_null_model_error);
    suite_add_tcase(s, tc_start_stream_errors);

    return s;
}

int main(void)
{
    Suite *s = anthropic_callbacks_vtable_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
