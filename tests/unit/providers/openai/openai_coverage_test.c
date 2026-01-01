/**
 * @file openai_coverage_test.c
 * @brief Coverage tests for openai.c error paths
 *
 * Tests error handling branches in start_request and start_stream.
 * Uses mock wrappers to inject failures in serialization, URL building,
 * header building, and HTTP multi operations.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/openai.h"
#include "providers/openai/openai_internal.h"
#include "providers/provider.h"
#include "wrapper_internal.h"
#include "error.h"

static TALLOC_CTX *test_ctx;

/* Mock control flags */
static bool g_serialize_chat_should_fail = false;
static bool g_serialize_responses_should_fail = false;
static bool g_build_chat_url_should_fail = false;
static bool g_build_responses_url_should_fail = false;
static bool g_build_headers_should_fail = false;
static bool g_http_multi_add_should_fail = false;

/* Dummy callbacks for tests */
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

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    /* Reset all mock flags */
    g_serialize_chat_should_fail = false;
    g_serialize_responses_should_fail = false;
    g_build_chat_url_should_fail = false;
    g_build_responses_url_should_fail = false;
    g_build_headers_should_fail = false;
    g_http_multi_add_should_fail = false;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Mock Implementations
 * ================================================================ */

res_t ik_openai_serialize_chat_request_(TALLOC_CTX *ctx, const ik_request_t *req,
                                        bool stream, char **out_json)
{
    (void)req;
    (void)stream;

    if (g_serialize_chat_should_fail) {
        return ERR(ctx, PARSE, "Mock chat serialize failure");
    }

    *out_json = talloc_strdup(ctx, "{\"model\":\"gpt-4\",\"messages\":[]}");
    return OK(*out_json);
}

res_t ik_openai_serialize_responses_request_(TALLOC_CTX *ctx, const ik_request_t *req,
                                             bool stream, char **out_json)
{
    (void)req;
    (void)stream;

    if (g_serialize_responses_should_fail) {
        return ERR(ctx, PARSE, "Mock responses serialize failure");
    }

    *out_json = talloc_strdup(ctx, "{\"model\":\"o1\",\"input\":\"test\"}");
    return OK(*out_json);
}

res_t ik_openai_build_chat_url_(TALLOC_CTX *ctx, const char *base_url, char **out_url)
{
    if (g_build_chat_url_should_fail) {
        return ERR(ctx, INVALID_ARG, "Mock chat URL build failure");
    }

    *out_url = talloc_asprintf(ctx, "%s/v1/chat/completions", base_url);
    return OK(*out_url);
}

res_t ik_openai_build_responses_url_(TALLOC_CTX *ctx, const char *base_url, char **out_url)
{
    if (g_build_responses_url_should_fail) {
        return ERR(ctx, INVALID_ARG, "Mock responses URL build failure");
    }

    *out_url = talloc_asprintf(ctx, "%s/v1/responses", base_url);
    return OK(*out_url);
}

res_t ik_openai_build_headers_(TALLOC_CTX *ctx, const char *api_key, char ***out_headers)
{
    (void)api_key;

    if (g_build_headers_should_fail) {
        return ERR(ctx, INVALID_ARG, "Mock headers build failure");
    }

    char **headers = talloc_array(ctx, char *, 3);
    headers[0] = talloc_strdup(headers, "Authorization: Bearer test");
    headers[1] = talloc_strdup(headers, "Content-Type: application/json");
    headers[2] = NULL;
    *out_headers = headers;
    return OK(headers);
}

res_t ik_http_multi_add_request_(void *http_multi, const void *http_req,
                                 void *write_cb, void *write_ctx,
                                 void *completion_cb, void *completion_ctx)
{
    (void)http_multi;
    (void)http_req;
    (void)write_cb;
    (void)write_ctx;
    (void)completion_cb;

    if (g_http_multi_add_should_fail) {
        return ERR(completion_ctx, IO, "Mock HTTP multi add failure");
    }

    return OK(NULL);
}

/* ================================================================
 * Helper: Create minimal request
 * ================================================================ */

static void setup_minimal_request(ik_request_t *req, ik_message_t *msg,
                                  ik_content_block_t *content, const char *model)
{
    content->type = IK_CONTENT_TEXT;
    content->data.text.text = talloc_strdup(test_ctx, "Test");

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
 * start_request Error Path Tests - Chat API
 * ================================================================ */

START_TEST(test_start_request_chat_serialize_failure) {
    /* Covers line 189: is_err(&serialize_res) for chat API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    g_serialize_chat_should_fail = true;

    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock chat serialize failure");
}
END_TEST

START_TEST(test_start_request_chat_url_failure) {
    /* Covers line 205: is_err(&url_res) for chat API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    g_build_chat_url_should_fail = true;

    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock chat URL build failure");
}
END_TEST

START_TEST(test_start_request_headers_failure) {
    /* Covers line 214: is_err(&headers_res) */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    g_build_headers_should_fail = true;

    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock headers build failure");
}
END_TEST

START_TEST(test_start_request_http_multi_add_failure) {
    /* Covers line 245: is_err(&add_res) */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    g_http_multi_add_should_fail = true;

    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock HTTP multi add failure");
}
END_TEST

/* ================================================================
 * start_request Error Path Tests - Responses API
 * ================================================================ */

START_TEST(test_start_request_responses_serialize_failure) {
    /* Covers line 189: is_err(&serialize_res) for responses API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test-key", true, &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "o1-preview");

    g_serialize_responses_should_fail = true;

    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock responses serialize failure");
}
END_TEST

START_TEST(test_start_request_responses_url_failure) {
    /* Covers line 205: is_err(&url_res) for responses API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test-key", true, &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "o1-preview");

    g_build_responses_url_should_fail = true;

    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock responses URL build failure");
}
END_TEST

/* ================================================================
 * start_stream Error Path Tests - Chat API
 * ================================================================ */

START_TEST(test_start_stream_chat_serialize_failure) {
    /* Covers line 299: is_err(&serialize_res) for chat API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    g_serialize_chat_should_fail = true;

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock chat serialize failure");
}
END_TEST

START_TEST(test_start_stream_chat_url_failure) {
    /* Covers line 315: is_err(&url_res) for chat API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    g_build_chat_url_should_fail = true;

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock chat URL build failure");
}
END_TEST

START_TEST(test_start_stream_headers_failure) {
    /* Covers line 324: is_err(&headers_res) */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    g_build_headers_should_fail = true;

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock headers build failure");
}
END_TEST

START_TEST(test_start_stream_http_multi_add_failure) {
    /* Covers line 353: is_err(&add_res) */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "gpt-4");

    g_http_multi_add_should_fail = true;

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock HTTP multi add failure");
}
END_TEST

/* ================================================================
 * start_stream Error Path Tests - Responses API
 * ================================================================ */

START_TEST(test_start_stream_responses_serialize_failure) {
    /* Covers line 299: is_err(&serialize_res) for responses API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test-key", true, &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "o1-preview");

    g_serialize_responses_should_fail = true;

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock responses serialize failure");
}
END_TEST

START_TEST(test_start_stream_responses_url_failure) {
    /* Covers line 315: is_err(&url_res) for responses API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test-key", true, &provider);
    ck_assert(is_ok(&r));

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    setup_minimal_request(&req, &msg, &content, "o1-preview");

    g_build_responses_url_should_fail = true;

    r = provider->vt->start_stream(provider->ctx, &req, dummy_stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Mock responses URL build failure");
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Coverage");

    TCase *tc_start_request = tcase_create("Start Request Error Paths");
    tcase_set_timeout(tc_start_request, 30);
    tcase_add_checked_fixture(tc_start_request, setup, teardown);
    tcase_add_test(tc_start_request, test_start_request_chat_serialize_failure);
    tcase_add_test(tc_start_request, test_start_request_chat_url_failure);
    tcase_add_test(tc_start_request, test_start_request_headers_failure);
    tcase_add_test(tc_start_request, test_start_request_http_multi_add_failure);
    tcase_add_test(tc_start_request, test_start_request_responses_serialize_failure);
    tcase_add_test(tc_start_request, test_start_request_responses_url_failure);
    suite_add_tcase(s, tc_start_request);

    TCase *tc_start_stream = tcase_create("Start Stream Error Paths");
    tcase_set_timeout(tc_start_stream, 30);
    tcase_add_checked_fixture(tc_start_stream, setup, teardown);
    tcase_add_test(tc_start_stream, test_start_stream_chat_serialize_failure);
    tcase_add_test(tc_start_stream, test_start_stream_chat_url_failure);
    tcase_add_test(tc_start_stream, test_start_stream_headers_failure);
    tcase_add_test(tc_start_stream, test_start_stream_http_multi_add_failure);
    tcase_add_test(tc_start_stream, test_start_stream_responses_serialize_failure);
    tcase_add_test(tc_start_stream, test_start_stream_responses_url_failure);
    suite_add_tcase(s, tc_start_stream);

    return s;
}

int main(void)
{
    Suite *s = openai_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
