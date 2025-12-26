/**
 * @file test_openai_streaming_vtable.c
 * @brief Unit tests for OpenAI provider vtable async streaming integration
 *
 * Tests the async streaming interface through the provider vtable.
 * Verifies fdset/perform/info_read pattern for event loop integration.
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include "providers/openai/openai.h"
#include "providers/provider.h"

static TALLOC_CTX *test_ctx;

/* Dummy completion callback for tests that need to call start_stream */
static res_t dummy_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)completion;
    (void)ctx;
    return OK(NULL);
}

/* Dummy stream callback */
static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Async Vtable Integration Tests
 * ================================================================ */

START_TEST(test_start_stream_returns_immediately) {
    /* Create provider instance */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key-12345", &provider);
    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(provider);

    /* Build minimal request */
    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_blocks = NULL,
        .content_count = 0,
        .provider_metadata = NULL
    };

    char *model_name = talloc_strdup(test_ctx, "gpt-4");

    ik_request_t req = {
        .system_prompt = NULL,
        .messages = &msg,
        .message_count = 1,
        .model = model_name,
        .thinking = { .level = IK_THINKING_NONE, .include_summary = false },
        .tools = NULL,
        .tool_count = 0,
        .max_output_tokens = 100,
        .tool_choice_mode = 0,
        .tool_choice_name = NULL
    };

    /* Test that start_stream returns immediately (non-blocking) */
    bool completion_called = false;
    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, &completion_called);

    /* Should return OK (request queued successfully) */
    ck_assert(!is_err(&r));

    /* Cleanup */
    talloc_free(provider);
}
END_TEST START_TEST(test_fdset_returns_valid_fds)
{
    /* Create provider instance */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key-12345", &provider);
    ck_assert(!is_err(&r));

    /* Test fdset before any requests */
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    r = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);

    /* Should return OK even with no active requests */
    ck_assert(!is_err(&r));
    /* max_fd should be -1 when no active transfers */
    ck_assert_int_eq(max_fd, -1);

    /* Cleanup */
    talloc_free(provider);
}

END_TEST START_TEST(test_perform_info_read_no_crash)
{
    /* Create provider instance */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key-12345", &provider);
    ck_assert(!is_err(&r));

    /* Test perform with no active requests */
    int running = 0;
    r = provider->vt->perform(provider->ctx, &running);

    /* Should return OK */
    ck_assert(!is_err(&r));
    /* No active requests */
    ck_assert_int_eq(running, 0);

    /* Test info_read with no completed transfers */
    provider->vt->info_read(provider->ctx, NULL);

    /* Should not crash - success means test passes */

    /* Cleanup */
    talloc_free(provider);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *openai_streaming_vtable_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Vtable");

    /* Async Vtable Integration */
    TCase *tc_async = tcase_create("AsyncVtable");
    tcase_add_checked_fixture(tc_async, setup, teardown);
    tcase_add_test(tc_async, test_start_stream_returns_immediately);
    tcase_add_test(tc_async, test_fdset_returns_valid_fds);
    tcase_add_test(tc_async, test_perform_info_read_no_crash);
    suite_add_tcase(s, tc_async);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_vtable_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
