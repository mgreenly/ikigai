#include "tests/test_constants.h"
/**
 * @file count_tokens_http_test.c
 * @brief Unit tests for ik_anthropic_count_tokens_http (real HTTP function)
 *
 * Tests the real curl-based HTTP function by mocking curl wrappers.
 * This covers count_tokens_write_cb and ik_anthropic_count_tokens_http
 * which are otherwise unreachable in unit tests.
 *
 * Covers:
 * - Success path: response body delivered via write callback
 * - curl_easy_init_ failure → ERR returned
 * - curl_easy_perform_ failure → ERR returned
 * - Large response body (> 4096 bytes) triggers write callback buffer growth
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

#include <check.h>
#include <curl/curl.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <talloc.h>
#include "apps/ikigai/providers/anthropic/count_tokens.h"
#include "shared/error.h"

/* ================================================================
 * Curl mock state
 * ================================================================ */

static bool g_curl_init_fail;
static CURLcode g_perform_result;
static long g_http_code;
static const char *g_response_body;

/* Captured write callback and userdata for response delivery */
typedef size_t (*write_fn_t)(const char *, size_t, size_t, void *);
static write_fn_t g_write_cb;
static void *g_write_ctx;

/* ================================================================
 * Curl mock implementations
 * ================================================================ */

CURL *curl_easy_init_(void)
{
    if (g_curl_init_fail) {
        return NULL;
    }
    return curl_easy_init();
}

CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, ...)
{
    va_list ap;
    va_start(ap, opt);

    CURLcode result = CURLE_OK;
    switch (opt) {
        case CURLOPT_WRITEFUNCTION: {
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

    /* Deliver mock response body via captured write callback */
    if (g_response_body != NULL && g_write_cb != NULL && g_write_ctx != NULL) {
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

void curl_easy_cleanup_(CURL *curl)
{
    curl_easy_cleanup(curl);
}

const char *curl_easy_strerror_(CURLcode code)
{
    return curl_easy_strerror(code);
}

struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string)
{
    return curl_slist_append(list, string);
}

void curl_slist_free_all_(struct curl_slist *list)
{
    curl_slist_free_all(list);
}

/* ================================================================
 * Fixtures
 * ================================================================ */

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    g_curl_init_fail = false;
    g_perform_result = CURLE_OK;
    g_http_code = 200;
    g_response_body = NULL;
    g_write_cb = NULL;
    g_write_ctx = NULL;
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

/* ================================================================
 * Tests
 * ================================================================ */

START_TEST(test_http_success) {
    /* Normal success: write callback delivers response body */
    g_response_body = "{\"input_tokens\": 42}";
    g_http_code = 200;

    char *response = NULL;
    long status = 0;
    res_t r = ik_anthropic_count_tokens_http(test_ctx,
                                              "https://api.anthropic.com/v1/messages/count_tokens",
                                              "test-api-key",
                                              "{\"model\":\"claude-3-5-sonnet-latest\"}",
                                              &response,
                                              &status);

    ck_assert_msg(!is_err(&r), "http function should return OK on success");
    ck_assert_int_eq(status, 200);
    ck_assert_ptr_nonnull(response);
    ck_assert_str_eq(response, "{\"input_tokens\": 42}");
}
END_TEST

START_TEST(test_http_curl_init_fail) {
    /* curl_easy_init_ returns NULL -> ERR */
    g_curl_init_fail = true;

    char *response = NULL;
    long status = 0;
    res_t r = ik_anthropic_count_tokens_http(test_ctx,
                                              "https://api.anthropic.com/v1/messages/count_tokens",
                                              "test-api-key",
                                              "{}",
                                              &response,
                                              &status);

    ck_assert_msg(is_err(&r), "http function should return ERR when curl init fails");
}
END_TEST

START_TEST(test_http_perform_fail) {
    /* curl_easy_perform_ fails -> ERR */
    g_perform_result = CURLE_COULDNT_CONNECT;

    char *response = NULL;
    long status = 0;
    res_t r = ik_anthropic_count_tokens_http(test_ctx,
                                              "https://api.anthropic.com/v1/messages/count_tokens",
                                              "test-api-key",
                                              "{}",
                                              &response,
                                              &status);

    ck_assert_msg(is_err(&r), "http function should return ERR when perform fails");
}
END_TEST

START_TEST(test_http_large_response_body) {
    /* Response body larger than cap*2 (>8192 bytes) triggers buffer growth
     * path where new_cap < new_len + 1, exercising line 45 in write callback */
    static char large_body[9000];
    memset(large_body, 'x', sizeof(large_body) - 50);
    /* End with valid JSON snippet */
    strcpy(large_body + sizeof(large_body) - 50, "\"end\":true}");
    large_body[0] = '{';
    large_body[sizeof(large_body) - 1] = '\0';

    g_response_body = large_body;
    g_http_code = 200;

    char *response = NULL;
    long status = 0;
    res_t r = ik_anthropic_count_tokens_http(test_ctx,
                                              "https://api.anthropic.com/v1/messages/count_tokens",
                                              "test-api-key",
                                              "{}",
                                              &response,
                                              &status);

    ck_assert_msg(!is_err(&r), "http function should return OK with large body");
    ck_assert_int_eq(status, 200);
    ck_assert_ptr_nonnull(response);
    ck_assert_int_eq((int)strlen(response), (int)strlen(large_body));
}
END_TEST

START_TEST(test_http_empty_response_body) {
    /* Empty response body is valid */
    g_response_body = NULL;
    g_http_code = 200;

    char *response = NULL;
    long status = 0;
    res_t r = ik_anthropic_count_tokens_http(test_ctx,
                                              "https://api.anthropic.com/v1/messages/count_tokens",
                                              "test-api-key",
                                              "{}",
                                              &response,
                                              &status);

    ck_assert_msg(!is_err(&r), "http function should return OK with empty body");
    ck_assert_int_eq(status, 200);
    ck_assert_ptr_nonnull(response);
}
END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *count_tokens_http_suite(void)
{
    Suite *s = suite_create("Anthropic count_tokens_http");

    TCase *tc = tcase_create("HTTP function");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_http_success);
    tcase_add_test(tc, test_http_curl_init_fail);
    tcase_add_test(tc, test_http_perform_fail);
    tcase_add_test(tc, test_http_large_response_body);
    tcase_add_test(tc, test_http_empty_response_body);
    suite_add_tcase(s, tc);

    return s;
}

#pragma GCC diagnostic pop

int main(void)
{
    Suite *s = count_tokens_http_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/providers/anthropic/count_tokens_http_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
