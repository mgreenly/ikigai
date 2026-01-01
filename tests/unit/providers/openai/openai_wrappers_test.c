/**
 * @file openai_wrappers_test.c
 * @brief Coverage test for openai.c wrapper functions
 *
 * This test exercises the wrapper functions (ik_openai_serialize_chat_request_,
 * etc.) that are defined in openai.c but only called internally. These functions
 * are exercised by calling start_request without providing mock implementations.
 *
 * We mock only the HTTP layer (ik_http_multi_add_request_) to prevent actual
 * network calls, but let the serialization/URL/header wrappers run normally.
 */

#include <check.h>
#include <talloc.h>
#include "providers/openai/openai.h"
#include "providers/provider.h"
#include "error.h"
#include "wrapper_internal.h"

static TALLOC_CTX *test_ctx;

/* ================================================================
 * Mock HTTP Layer
 * ================================================================ */

/* Mock ik_http_multi_add_request_ to prevent actual HTTP calls */
res_t ik_http_multi_add_request_(void *http_multi, const void *http_req,
                                 void *write_cb, void *write_ctx,
                                 void *completion_cb, void *completion_ctx)
{
    (void)http_multi;
    (void)http_req;
    (void)write_cb;
    (void)write_ctx;
    (void)completion_cb;
    (void)completion_ctx;

    /* Always succeed - we're just testing serialization/URL/header building */
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

/* Dummy callbacks */
static res_t dummy_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)completion;
    (void)ctx;
    return OK(NULL);
}

static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

/* ================================================================
 * Helper: Create minimal request
 * ================================================================ */

static void setup_minimal_request(ik_request_t *req, ik_message_t *msg,
                                  ik_content_block_t *content, const char *model)
{
    content->type = IK_CONTENT_TEXT;
    content->data.text.text = talloc_strdup(test_ctx, "Test message");

    msg->role = IK_ROLE_USER;
    msg->content_blocks = content;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    req->system_prompt = NULL;
    req->messages = msg;
    req->message_count = 1;
    req->model = talloc_strdup(test_ctx, model);
    req->thinking.level = IK_THINKING_NONE;
    req->thinking.include_summary = false;
    req->tools = NULL;
    req->tool_count = 0;
    req->max_output_tokens = 100;
    req->tool_choice_mode = 0;
    req->tool_choice_name = NULL;
}

/* ================================================================
 * Wrapper Function Coverage Tests
 * ================================================================ */

START_TEST(test_wrappers_via_start_request_chat) {
    /* This test calls start_request with a chat model, which exercises:
     * - ik_openai_serialize_chat_request_
     * - ik_openai_build_chat_url_
     * - ik_openai_build_headers_
     */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    /* We expect OK - the request is queued successfully */
    ck_assert(is_ok(&r));
}
END_TEST

START_TEST(test_wrappers_via_start_request_responses) {
    /* This test calls start_request with responses API, which exercises:
     * - ik_openai_serialize_responses_request_
     * - ik_openai_build_responses_url_
     * - ik_openai_build_headers_
     */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test-key", true, &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "o1-preview");

    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    /* We expect OK - the request is queued successfully */
    ck_assert(is_ok(&r));
}
END_TEST

START_TEST(test_wrappers_via_start_stream_chat) {
    /* This test calls start_stream with a chat model */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    /* We expect OK - the stream request is queued successfully */
    ck_assert(is_ok(&r));
}
END_TEST

START_TEST(test_wrappers_via_start_stream_responses) {
    /* This test calls start_stream with responses API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test-key", true, &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "o1-preview");

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    /* We expect OK - the stream request is queued successfully */
    ck_assert(is_ok(&r));
}
END_TEST

START_TEST(test_auto_prefer_responses_api_start_request) {
    /* Test that o1 model auto-selects responses API even without use_responses_api flag
     * This exercises the "|| ik_openai_prefer_responses_api(req->model)" branch */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "o1-preview");

    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    /* We expect OK - should use responses API automatically */
    ck_assert(is_ok(&r));
}
END_TEST

START_TEST(test_auto_prefer_responses_api_start_stream) {
    /* Test that o1 model auto-selects responses API even without use_responses_api flag
     * This exercises the "|| ik_openai_prefer_responses_api(req->model)" branch */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "o1-preview");

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    /* We expect OK - should use responses API automatically */
    ck_assert(is_ok(&r));
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_wrappers_suite(void)
{
    Suite *s = suite_create("OpenAI Wrappers Coverage");

    TCase *tc_wrappers = tcase_create("Wrapper Functions");
    tcase_set_timeout(tc_wrappers, 30);
    tcase_add_checked_fixture(tc_wrappers, setup, teardown);
    tcase_add_test(tc_wrappers, test_wrappers_via_start_request_chat);
    tcase_add_test(tc_wrappers, test_wrappers_via_start_request_responses);
    tcase_add_test(tc_wrappers, test_wrappers_via_start_stream_chat);
    tcase_add_test(tc_wrappers, test_wrappers_via_start_stream_responses);
    tcase_add_test(tc_wrappers, test_auto_prefer_responses_api_start_request);
    tcase_add_test(tc_wrappers, test_auto_prefer_responses_api_start_stream);
    suite_add_tcase(s, tc_wrappers);

    return s;
}

int main(void)
{
    Suite *s = openai_wrappers_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
