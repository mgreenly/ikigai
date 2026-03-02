#include "tests/test_constants.h"
/**
 * @file count_tokens_test.c
 * @brief Unit tests for Anthropic count_tokens vtable method
 *
 * Tests:
 * - Successful token count returned from API response
 * - API error (non-2xx) falls back to bytes estimator
 * - Network error falls back to bytes estimator
 * - Malformed response falls back to bytes estimator
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/anthropic.h"
#include "apps/ikigai/providers/anthropic/anthropic_internal.h"
#include "apps/ikigai/providers/common/http_multi.h"
#include "apps/ikigai/providers/common/http_multi_internal.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/wrapper.h"
#include "shared/wrapper_internal.h"
#include "shared/error.h"

/* ================================================================
 * Mock state
 * ================================================================ */

static const char *g_mock_response_body = NULL;
static long g_mock_http_status = 200;
static bool g_mock_http_error = false;    /* If true, return ERR */

/* ================================================================
 * Mock: ik_http_multi_create_ (needed for ik_anthropic_create)
 * ================================================================ */

/* Forward declarations for http_multi internals */
typedef struct ik_http_multi ik_http_multi_t;

res_t ik_http_multi_create_(void *parent, void **out)
{
    ik_http_multi_t *stub = talloc_zero(parent, ik_http_multi_t);
    *out = stub;
    return OK(stub);
}

void ik_http_multi_info_read_(void *http_multi, void *logger)
{
    (void)http_multi;
    (void)logger;
}

/* ================================================================
 * Mock: ik_anthropic_count_tokens_http_ (the HTTP call)
 * ================================================================ */

res_t ik_anthropic_count_tokens_http_(TALLOC_CTX *ctx,
                                      const char *url,
                                      const char *api_key,
                                      const char *body,
                                      char **response_out,
                                      long *http_status_out)
{
    (void)url;
    (void)api_key;
    (void)body;

    if (g_mock_http_error) {
        return ERR(ctx, IO, "mock network error");
    }

    *http_status_out = g_mock_http_status;
    *response_out = talloc_strdup(ctx, g_mock_response_body ? g_mock_response_body : "");
    return OK(NULL);
}

/* ================================================================
 * Test fixtures
 * ================================================================ */

static TALLOC_CTX *test_ctx;
static ik_provider_t *provider;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    g_mock_response_body = NULL;
    g_mock_http_status = 200;
    g_mock_http_error = false;

    res_t r = ik_anthropic_create(test_ctx, "test-api-key", &provider);
    ck_assert_msg(!is_err(&r), "Failed to create provider");
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
    provider = NULL;
}

/* ================================================================
 * Helpers
 * ================================================================ */

static ik_request_t *make_request(TALLOC_CTX *ctx)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(ctx, "claude-sonnet-4-6");
    req->system_prompt = talloc_strdup(ctx, "You are helpful");

    req->messages = talloc_zero_array(ctx, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(ctx, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(ctx, "Hello, world");

    return req;
}

/* ================================================================
 * Tests: Successful count
 * ================================================================ */

START_TEST(test_count_tokens_success)
{
    g_mock_response_body = "{\"input_tokens\": 42}";
    g_mock_http_status = 200;

    ik_request_t *req = make_request(test_ctx);
    int32_t count = 0;

    res_t r = provider->vt->count_tokens(provider->ctx, req, &count);

    ck_assert_msg(!is_err(&r), "count_tokens should not return error");
    ck_assert_int_eq(count, 42);
}
END_TEST

START_TEST(test_count_tokens_large_value)
{
    g_mock_response_body = "{\"input_tokens\": 99999}";
    g_mock_http_status = 200;

    ik_request_t *req = make_request(test_ctx);
    int32_t count = 0;

    res_t r = provider->vt->count_tokens(provider->ctx, req, &count);

    ck_assert_msg(!is_err(&r), "count_tokens should not return error");
    ck_assert_int_eq(count, 99999);
}
END_TEST

/* ================================================================
 * Tests: API error fallback
 * ================================================================ */

START_TEST(test_count_tokens_api_error_401)
{
    /* 401 Unauthorized -> fallback to bytes estimator */
    g_mock_response_body = "{\"error\": \"unauthorized\"}";
    g_mock_http_status = 401;

    ik_request_t *req = make_request(test_ctx);
    int32_t count = 0;

    res_t r = provider->vt->count_tokens(provider->ctx, req, &count);

    ck_assert_msg(!is_err(&r), "count_tokens should return OK even on API error");
    /* Fallback: bytes estimate from serialized body, must be positive */
    ck_assert_int_gt(count, 0);
}
END_TEST

START_TEST(test_count_tokens_api_error_429)
{
    /* 429 Rate limit -> fallback */
    g_mock_response_body = "{\"error\": \"rate_limit\"}";
    g_mock_http_status = 429;

    ik_request_t *req = make_request(test_ctx);
    int32_t count = 0;

    res_t r = provider->vt->count_tokens(provider->ctx, req, &count);

    ck_assert_msg(!is_err(&r), "count_tokens should return OK on rate limit");
    ck_assert_int_gt(count, 0);
}
END_TEST

START_TEST(test_count_tokens_network_error)
{
    /* Network failure -> fallback */
    g_mock_http_error = true;

    ik_request_t *req = make_request(test_ctx);
    int32_t count = 0;

    res_t r = provider->vt->count_tokens(provider->ctx, req, &count);

    ck_assert_msg(!is_err(&r), "count_tokens should return OK on network error");
    ck_assert_int_gt(count, 0);
}
END_TEST

/* ================================================================
 * Tests: Malformed response fallback
 * ================================================================ */

START_TEST(test_count_tokens_malformed_json)
{
    g_mock_response_body = "not valid json";
    g_mock_http_status = 200;

    ik_request_t *req = make_request(test_ctx);
    int32_t count = 0;

    res_t r = provider->vt->count_tokens(provider->ctx, req, &count);

    ck_assert_msg(!is_err(&r), "count_tokens should return OK on parse error");
    ck_assert_int_gt(count, 0);
}
END_TEST

START_TEST(test_count_tokens_missing_field)
{
    /* Response has no input_tokens field */
    g_mock_response_body = "{\"something_else\": 10}";
    g_mock_http_status = 200;

    ik_request_t *req = make_request(test_ctx);
    int32_t count = 0;

    res_t r = provider->vt->count_tokens(provider->ctx, req, &count);

    ck_assert_msg(!is_err(&r), "count_tokens should return OK on missing field");
    ck_assert_int_gt(count, 0);
}
END_TEST

START_TEST(test_count_tokens_empty_response)
{
    g_mock_response_body = "";
    g_mock_http_status = 200;

    ik_request_t *req = make_request(test_ctx);
    int32_t count = 0;

    res_t r = provider->vt->count_tokens(provider->ctx, req, &count);

    ck_assert_msg(!is_err(&r), "count_tokens should return OK on empty response");
    ck_assert_int_gt(count, 0);
}
END_TEST

/* ================================================================
 * Test: vtable entry is non-NULL
 * ================================================================ */

START_TEST(test_count_tokens_vtable_non_null)
{
    ck_assert_ptr_nonnull(provider->vt->count_tokens);
}
END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *count_tokens_suite(void)
{
    Suite *s = suite_create("Anthropic count_tokens");

    TCase *tc_success = tcase_create("Success");
    tcase_set_timeout(tc_success, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_success, setup, teardown);
    tcase_add_test(tc_success, test_count_tokens_success);
    tcase_add_test(tc_success, test_count_tokens_large_value);
    tcase_add_test(tc_success, test_count_tokens_vtable_non_null);
    suite_add_tcase(s, tc_success);

    TCase *tc_api_error = tcase_create("API Error Fallback");
    tcase_set_timeout(tc_api_error, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_api_error, setup, teardown);
    tcase_add_test(tc_api_error, test_count_tokens_api_error_401);
    tcase_add_test(tc_api_error, test_count_tokens_api_error_429);
    tcase_add_test(tc_api_error, test_count_tokens_network_error);
    suite_add_tcase(s, tc_api_error);

    TCase *tc_malformed = tcase_create("Malformed Response Fallback");
    tcase_set_timeout(tc_malformed, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_malformed, setup, teardown);
    tcase_add_test(tc_malformed, test_count_tokens_malformed_json);
    tcase_add_test(tc_malformed, test_count_tokens_missing_field);
    tcase_add_test(tc_malformed, test_count_tokens_empty_response);
    suite_add_tcase(s, tc_malformed);

    return s;
}

int main(void)
{
    Suite *s = count_tokens_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/count_tokens_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
