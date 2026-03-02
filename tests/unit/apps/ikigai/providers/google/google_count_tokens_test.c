#include "tests/test_constants.h"
/**
 * @file google_count_tokens_test.c
 * @brief Unit tests for Google provider count_tokens vtable method
 *
 * Tests the synchronous countTokens API call with mock curl functions
 * that override the weak symbols from wrapper_curl.c.
 */

#include <check.h>
#include <talloc.h>
#include <stdarg.h>
#include <string.h>
#include <curl/curl.h>

#include "apps/ikigai/providers/google/google.h"
#include "apps/ikigai/providers/google/count_tokens.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"

/* ================================================================
 * Mock curl function forward declarations (required for -Wmissing-prototypes)
 * ================================================================ */

CURL *curl_easy_init_(void);
void curl_easy_cleanup_(CURL *curl);
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, ...);
CURLcode curl_easy_perform_(CURL *curl);
CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...);
const char *curl_easy_strerror_(CURLcode code);
struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string);
void curl_slist_free_all_(struct curl_slist *list);

/* ================================================================
 * Mock curl state
 * ================================================================ */

static long g_mock_http_status = 200;
static const char *g_mock_response_body = "{\"totalTokens\":42}";
static CURLcode g_mock_perform_result = CURLE_OK;

/* Track write callback registered via CURLOPT_WRITEFUNCTION/WRITEDATA */
static size_t (*g_mock_write_cb)(char *, size_t, size_t, void *) = NULL;
static void *g_mock_write_data = NULL;

/* Fake slist node to return from curl_slist_append_ */
static struct curl_slist g_fake_slist;

/* ================================================================
 * Mock curl function overrides (strong symbols override weak)
 * ================================================================ */

CURL *curl_easy_init_(void)
{
    return (CURL *)1; /* Non-null fake handle */
}

void curl_easy_cleanup_(CURL *curl)
{
    (void)curl;
}

CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, ...)
{
    (void)curl;
    va_list args;
    va_start(args, opt);
    if (opt == CURLOPT_WRITEFUNCTION) {
        g_mock_write_cb = va_arg(args, size_t (*)(char *, size_t, size_t, void *));
    } else if (opt == CURLOPT_WRITEDATA) {
        g_mock_write_data = va_arg(args, void *);
    } else {
        (void)va_arg(args, void *);
    }
    va_end(args);
    return CURLE_OK;
}

CURLcode curl_easy_perform_(CURL *curl)
{
    (void)curl;
    if (g_mock_perform_result != CURLE_OK) {
        return g_mock_perform_result;
    }
    if (g_mock_write_cb != NULL && g_mock_response_body != NULL) {
        size_t len = strlen(g_mock_response_body);
        /* Curl write callback uses char* not const char*. Suppress cast-qual. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        g_mock_write_cb((char *)g_mock_response_body, len, 1, g_mock_write_data);
#pragma GCC diagnostic pop
    }
    return CURLE_OK;
}

CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...)
{
    (void)curl;
    va_list args;
    va_start(args, info);
    if (info == CURLINFO_RESPONSE_CODE) {
        long *out = va_arg(args, long *);
        *out = g_mock_http_status;
    } else {
        (void)va_arg(args, void *);
    }
    va_end(args);
    return CURLE_OK;
}

const char *curl_easy_strerror_(CURLcode code)
{
    (void)code;
    return "mock curl error";
}

struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string)
{
    (void)list;
    (void)string;
    return &g_fake_slist; /* Non-null: success */
}

void curl_slist_free_all_(struct curl_slist *list)
{
    (void)list;
}

/* ================================================================
 * Test fixtures
 * ================================================================ */

static TALLOC_CTX *test_ctx;
static ik_provider_t *provider;
static ik_request_t *request;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    /* Create provider with test key */
    res_t r = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&r));

    /* Create a basic request */
    request = talloc_zero(test_ctx, ik_request_t);
    request->model = talloc_strdup(request, "gemini-2.5-flash");
    request->max_output_tokens = 1024;

    request->messages = talloc_zero_array(request, ik_message_t, 1);
    request->message_count = 1;
    request->messages[0].role = IK_ROLE_USER;
    request->messages[0].content_blocks = talloc_zero_array(request, ik_content_block_t, 1);
    request->messages[0].content_count = 1;
    request->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    request->messages[0].content_blocks[0].data.text.text =
        talloc_strdup(request, "Hello, how are you?");

    /* Reset mock state */
    g_mock_http_status = 200;
    g_mock_response_body = "{\"totalTokens\":42}";
    g_mock_perform_result = CURLE_OK;
    g_mock_write_cb = NULL;
    g_mock_write_data = NULL;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Tests: vtable wiring
 * ================================================================ */

START_TEST(test_count_tokens_vtable_non_null) {
    ck_assert_ptr_nonnull(provider->vt->count_tokens);
}
END_TEST

/* ================================================================
 * Tests: successful API call
 * ================================================================ */

START_TEST(test_count_tokens_success_returns_total_tokens) {
    g_mock_http_status = 200;
    g_mock_response_body = "{\"totalTokens\":42}";
    g_mock_perform_result = CURLE_OK;

    int32_t token_count = 0;
    res_t r = provider->vt->count_tokens(provider->ctx, request, &token_count);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(token_count, 42);
}
END_TEST

START_TEST(test_count_tokens_success_larger_count) {
    g_mock_http_status = 200;
    g_mock_response_body = "{\"totalTokens\":1234,\"promptTokensDetails\":[]}";
    g_mock_perform_result = CURLE_OK;

    int32_t token_count = 0;
    res_t r = provider->vt->count_tokens(provider->ctx, request, &token_count);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(token_count, 1234);
}
END_TEST

/* ================================================================
 * Tests: API error fallback
 * ================================================================ */

START_TEST(test_count_tokens_http_403_falls_back_to_bytes_estimate) {
    g_mock_http_status = 403;
    g_mock_response_body = "{\"error\":{\"code\":403,\"message\":\"API key invalid\"}}";
    g_mock_perform_result = CURLE_OK;

    int32_t token_count = 0;
    res_t r = provider->vt->count_tokens(provider->ctx, request, &token_count);

    /* Always returns OK (fallback is transparent) */
    ck_assert(!is_err(&r));
    /* Bytes estimate is non-negative */
    ck_assert_int_ge(token_count, 0);
}
END_TEST

START_TEST(test_count_tokens_http_429_falls_back_to_bytes_estimate) {
    g_mock_http_status = 429;
    g_mock_response_body = "{\"error\":{\"code\":429,\"message\":\"Rate limit exceeded\"}}";
    g_mock_perform_result = CURLE_OK;

    int32_t token_count = 0;
    res_t r = provider->vt->count_tokens(provider->ctx, request, &token_count);

    ck_assert(!is_err(&r));
    ck_assert_int_ge(token_count, 0);
}
END_TEST

START_TEST(test_count_tokens_network_error_falls_back_to_bytes_estimate) {
    g_mock_perform_result = CURLE_COULDNT_CONNECT;
    g_mock_response_body = NULL;

    int32_t token_count = 0;
    res_t r = provider->vt->count_tokens(provider->ctx, request, &token_count);

    ck_assert(!is_err(&r));
    ck_assert_int_ge(token_count, 0);
}
END_TEST

/* ================================================================
 * Tests: malformed response fallback
 * ================================================================ */

START_TEST(test_count_tokens_malformed_json_falls_back_to_bytes_estimate) {
    g_mock_http_status = 200;
    g_mock_response_body = "not valid json at all!!!";
    g_mock_perform_result = CURLE_OK;

    int32_t token_count = 0;
    res_t r = provider->vt->count_tokens(provider->ctx, request, &token_count);

    ck_assert(!is_err(&r));
    ck_assert_int_ge(token_count, 0);
}
END_TEST

START_TEST(test_count_tokens_missing_total_tokens_field_falls_back) {
    g_mock_http_status = 200;
    /* Valid JSON but no totalTokens field */
    g_mock_response_body = "{\"otherField\":99}";
    g_mock_perform_result = CURLE_OK;

    int32_t token_count = 0;
    res_t r = provider->vt->count_tokens(provider->ctx, request, &token_count);

    ck_assert(!is_err(&r));
    ck_assert_int_ge(token_count, 0);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_count_tokens_suite(void)
{
    Suite *s = suite_create("Google count_tokens");

    TCase *tc_vtable = tcase_create("vtable");
    tcase_set_timeout(tc_vtable, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_vtable, setup, teardown);
    tcase_add_test(tc_vtable, test_count_tokens_vtable_non_null);
    suite_add_tcase(s, tc_vtable);

    TCase *tc_success = tcase_create("success");
    tcase_set_timeout(tc_success, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_success, setup, teardown);
    tcase_add_test(tc_success, test_count_tokens_success_returns_total_tokens);
    tcase_add_test(tc_success, test_count_tokens_success_larger_count);
    suite_add_tcase(s, tc_success);

    TCase *tc_api_error = tcase_create("api_error");
    tcase_set_timeout(tc_api_error, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_api_error, setup, teardown);
    tcase_add_test(tc_api_error, test_count_tokens_http_403_falls_back_to_bytes_estimate);
    tcase_add_test(tc_api_error, test_count_tokens_http_429_falls_back_to_bytes_estimate);
    tcase_add_test(tc_api_error, test_count_tokens_network_error_falls_back_to_bytes_estimate);
    suite_add_tcase(s, tc_api_error);

    TCase *tc_malformed = tcase_create("malformed_response");
    tcase_set_timeout(tc_malformed, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_malformed, setup, teardown);
    tcase_add_test(tc_malformed, test_count_tokens_malformed_json_falls_back_to_bytes_estimate);
    tcase_add_test(tc_malformed, test_count_tokens_missing_total_tokens_field_falls_back);
    suite_add_tcase(s, tc_malformed);

    return s;
}

int main(void)
{
    Suite *s = google_count_tokens_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/providers/google/google_count_tokens_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
