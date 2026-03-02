#include "tests/test_constants.h"
/**
 * @file openai_count_tokens_test.c
 * @brief Unit tests for OpenAI count_tokens vtable method
 *
 * Tests the count_tokens vtable entry which makes a synchronous POST to
 * /v1/responses/input_tokens. Uses curl mocks to avoid real network calls.
 *
 * Covers:
 * - Successful token count returned from API
 * - API error (curl error) falls back to bytes estimate
 * - HTTP error (non-200) falls back to bytes estimate
 * - Malformed JSON response falls back to bytes estimate
 * - Missing input_tokens field falls back to bytes estimate
 * - curl_easy_init failure falls back to bytes estimate
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

#include <check.h>
#include <curl/curl.h>
#include <string.h>
#include <talloc.h>
#include "apps/ikigai/providers/openai/openai.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"
#include "shared/error.h"

static TALLOC_CTX *test_ctx;
static ik_provider_t *provider;

/* ================================================================
 * Curl mock state
 * ================================================================ */

/* Control variables */
static bool g_curl_init_should_fail = false;
static CURLcode g_perform_result = CURLE_OK;
static long g_http_code = 200;
static const char *g_response_body = NULL;

/* Captured setopt values for write callback injection */
typedef size_t (*write_fn_t)(const char *, size_t, size_t, void *);
static write_fn_t g_write_cb = NULL;
static void *g_write_ctx = NULL;

/* ================================================================
 * Curl mock implementations
 * ================================================================ */

CURL *curl_easy_init_(void)
{
    if (g_curl_init_should_fail) {
        return NULL;
    }
    return curl_easy_init();
}

CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, ...)
{
    /* Capture the write callback and data pointer */
    va_list ap;
    va_start(ap, opt);

    CURLcode result = CURLE_OK;
    switch (opt) {
        case CURLOPT_WRITEFUNCTION: {
            /* cast through void* to avoid pedantic function pointer warnings */
            void *fn_ptr = va_arg(ap, void *);
            g_write_cb = (write_fn_t)(uintptr_t)fn_ptr;
            result = curl_easy_setopt(curl, opt, fn_ptr);
            break;
        }
        case CURLOPT_WRITEDATA:
            g_write_ctx = va_arg(ap, void *);
            result = curl_easy_setopt(curl, opt, g_write_ctx);
            break;
        default: {
            void *val = va_arg(ap, void *);
            result = curl_easy_setopt(curl, opt, val);
            break;
        }
    }
    va_end(ap);
    return result;
}

CURLcode curl_easy_perform_(CURL *curl)
{
    (void)curl;

    if (g_perform_result != CURLE_OK) {
        return g_perform_result;
    }

    /* Deliver mock response body via write callback */
    if (g_response_body && g_write_cb && g_write_ctx) {
        size_t len = strlen(g_response_body);
        g_write_cb(g_response_body, 1, len, g_write_ctx);
    }

    return CURLE_OK;
}

CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...)
{
    va_list ap;
    va_start(ap, info);

    CURLcode result = CURLE_OK;
    if (info == CURLINFO_RESPONSE_CODE) {
        long *code_ptr = va_arg(ap, long *);
        *code_ptr = g_http_code;
    } else {
        void *ptr = va_arg(ap, void *);
        result = curl_easy_getinfo(curl, info, ptr);
    }
    va_end(ap);
    return result;
}

/* ================================================================
 * Helpers
 * ================================================================ */

static void reset_mocks(void)
{
    g_curl_init_should_fail = false;
    g_perform_result = CURLE_OK;
    g_http_code = 200;
    g_response_body = NULL;
    g_write_cb = NULL;
    g_write_ctx = NULL;
}

static void build_request(ik_request_t *req, ik_message_t *msg, ik_content_block_t *content)
{
    content->type = IK_CONTENT_TEXT;
    content->data.text.text = talloc_strdup(test_ctx, "Hello, world!");

    msg->role = IK_ROLE_USER;
    msg->content_blocks = content;
    msg->content_count = 1;
    msg->provider_metadata = NULL;
    msg->interrupted = false;

    req->system_prompt = NULL;
    req->messages = msg;
    req->message_count = 1;
    req->model = talloc_strdup(test_ctx, "gpt-4o");
    req->thinking.level = IK_THINKING_MIN;
    req->thinking.include_summary = false;
    req->tools = NULL;
    req->tool_count = 0;
    req->max_output_tokens = 0;
    req->tool_choice_mode = 0;
    req->tool_choice_name = NULL;
}

/* ================================================================
 * Fixtures
 * ================================================================ */

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    reset_mocks();
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(!is_err(&r));
}

static void teardown(void)
{
    talloc_free(test_ctx);
    reset_mocks();
}

/* ================================================================
 * Tests
 * ================================================================ */

/**
 * Successful token count returned from API.
 */
START_TEST(test_count_tokens_success) {
    g_response_body = "{\"object\":\"response.input_tokens\",\"input_tokens\":42}";

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    build_request(&req, &msg, &content);

    int32_t count = -1;
    res_t r = provider->vt->count_tokens(provider->ctx, &req, &count);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(count, 42);
}
END_TEST

/**
 * API error (curl error) falls back to bytes estimate and returns OK.
 */
START_TEST(test_count_tokens_curl_error_fallback) {
    g_perform_result = CURLE_COULDNT_CONNECT;

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    build_request(&req, &msg, &content);

    int32_t count = -1;
    res_t r = provider->vt->count_tokens(provider->ctx, &req, &count);

    /* Must return OK (not ERR) with the bytes estimate */
    ck_assert(!is_err(&r));
    ck_assert_int_ge(count, 0);
}
END_TEST

/**
 * HTTP error (non-2xx) falls back to bytes estimate and returns OK.
 */
START_TEST(test_count_tokens_http_error_fallback) {
    g_http_code = 429;
    g_response_body = "{\"error\":{\"message\":\"Rate limited\"}}";

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    build_request(&req, &msg, &content);

    int32_t count = -1;
    res_t r = provider->vt->count_tokens(provider->ctx, &req, &count);

    ck_assert(!is_err(&r));
    ck_assert_int_ge(count, 0);
}
END_TEST

/**
 * Malformed JSON response falls back to bytes estimate and returns OK.
 */
START_TEST(test_count_tokens_malformed_json_fallback) {
    g_response_body = "not-json-at-all{{{{";

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    build_request(&req, &msg, &content);

    int32_t count = -1;
    res_t r = provider->vt->count_tokens(provider->ctx, &req, &count);

    ck_assert(!is_err(&r));
    ck_assert_int_ge(count, 0);
}
END_TEST

/**
 * JSON response missing input_tokens field falls back to bytes estimate.
 */
START_TEST(test_count_tokens_missing_field_fallback) {
    g_response_body = "{\"object\":\"response.input_tokens\"}";

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    build_request(&req, &msg, &content);

    int32_t count = -1;
    res_t r = provider->vt->count_tokens(provider->ctx, &req, &count);

    ck_assert(!is_err(&r));
    ck_assert_int_ge(count, 0);
}
END_TEST

/**
 * curl_easy_init failure falls back to bytes estimate.
 */
START_TEST(test_count_tokens_curl_init_fail_fallback) {
    g_curl_init_should_fail = true;

    ik_request_t req;
    ik_message_t msg;
    ik_content_block_t content;
    build_request(&req, &msg, &content);

    int32_t count = -1;
    res_t r = provider->vt->count_tokens(provider->ctx, &req, &count);

    ck_assert(!is_err(&r));
    ck_assert_int_ge(count, 0);
}
END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *openai_count_tokens_suite(void)
{
    Suite *s = suite_create("OpenAI count_tokens");

    TCase *tc = tcase_create("count_tokens");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_count_tokens_success);
    tcase_add_test(tc, test_count_tokens_curl_error_fallback);
    tcase_add_test(tc, test_count_tokens_http_error_fallback);
    tcase_add_test(tc, test_count_tokens_malformed_json_fallback);
    tcase_add_test(tc, test_count_tokens_missing_field_fallback);
    tcase_add_test(tc, test_count_tokens_curl_init_fail_fallback);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_count_tokens_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/providers/openai/openai_count_tokens_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
