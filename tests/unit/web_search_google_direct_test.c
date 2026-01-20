#include "web_search_google.h"

#include "http_utils.h"
#include "json_allocator.h"
#include "panic.h"
#include "response_processor.h"
#include "wrapper_web.h"

#include <check.h>
#include <curl/curl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "vendor/yyjson/yyjson.h"

static void *test_ctx = NULL;
static CURL *mock_curl_return = NULL;
static CURLcode mock_perform_return = CURLE_OK;
static CURLM *mock_multi_handle_return = NULL;
static char *mock_url_encode_return = NULL;
static char *mock_process_responses_return = NULL;
static int64_t mock_http_code = 200;
static char *mock_response_data = NULL;
static char *mock_google_search_api_key = NULL;
static char *mock_google_search_engine_id = NULL;

// Captured from curl_easy_setopt_
static size_t (*captured_write_callback)(void *, size_t, size_t, void *) = NULL;
static void *captured_write_data = NULL;

// Forward declarations of helper functions
void output_error_with_event(void *ctx, const char *msg, const char *code);
void output_error(void *ctx, const char *msg, const char *code);
char *url_encode(void *ctx, const char *str);
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
char *process_responses(void *ctx,
                        struct api_call *calls,
                        size_t num_calls,
                        size_t allowed_count,
                        size_t blocked_count,
                        yyjson_val *blocked_domains_val,
                        int64_t num);
const char *curl_easy_strerror_(CURLcode code);

// Mock implementations
char *getenv_(const char *name)
{
    if (strcmp(name, "GOOGLE_SEARCH_API_KEY") == 0) {
        return mock_google_search_api_key;
    }
    if (strcmp(name, "GOOGLE_SEARCH_ENGINE_ID") == 0) {
        return mock_google_search_engine_id;
    }
    return NULL;
}

void output_error_with_event(void *ctx, const char *msg, const char *code)
{
    (void)ctx;
    (void)msg;
    (void)code;
}

void output_error(void *ctx, const char *msg, const char *code)
{
    (void)ctx;
    (void)msg;
    (void)code;
}

char *url_encode(void *ctx, const char *str)
{
    if (mock_url_encode_return == NULL) {
        return talloc_strdup(ctx, str);
    }
    return talloc_strdup(ctx, mock_url_encode_return);
}

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct response_buffer *mem = (struct response_buffer *)userp;

    char *ptr = talloc_realloc(mem->ctx, mem->data, char, (unsigned int)(mem->size + realsize + 1));
    if (ptr == NULL) {
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';

    return realsize;
}

char *process_responses(void *ctx,
                        struct api_call *calls,
                        size_t num_calls,
                        size_t allowed_count,
                        size_t blocked_count,
                        yyjson_val *blocked_domains_val,
                        int64_t num)
{
    (void)calls;
    (void)num_calls;
    (void)allowed_count;
    (void)blocked_count;
    (void)blocked_domains_val;
    (void)num;
    if (mock_process_responses_return == NULL) {
        return talloc_strdup(ctx, "{\"success\": true, \"results\": []}");
    }
    return talloc_strdup(ctx, mock_process_responses_return);
}

const char *curl_easy_strerror_(CURLcode code)
{
    (void)code;
    return "mock error";
}

CURL *curl_easy_init_(void)
{
    return mock_curl_return;
}

CURLcode curl_easy_perform_(CURL *curl)
{
    (void)curl;
    if (mock_perform_return == CURLE_OK && mock_response_data != NULL && captured_write_callback != NULL &&
        captured_write_data != NULL) {
        size_t len = strlen(mock_response_data);
        captured_write_callback(mock_response_data, 1, len, captured_write_data);
    }
    return mock_perform_return;
}

void curl_easy_cleanup_(CURL *curl)
{
    (void)curl;
}

CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...)
{
    (void)curl;
    if (info == CURLINFO_RESPONSE_CODE) {
        va_list args;
        va_start(args, info);
        int64_t *code_ptr = va_arg(args, int64_t *);
        va_end(args);
        *code_ptr = mock_http_code;
    }
    return CURLE_OK;
}

CURLcode curl_easy_setopt_(CURL *curl, CURLoption option, ...)
{
    (void)curl;
    va_list args;
    va_start(args, option);

    if (option == CURLOPT_WRITEFUNCTION) {
        captured_write_callback = va_arg(args, size_t (*)(void *, size_t, size_t, void *));
    } else if (option == CURLOPT_WRITEDATA) {
        captured_write_data = va_arg(args, void *);
    }

    va_end(args);
    return CURLE_OK;
}

CURLM *curl_multi_init_(void)
{
    return mock_multi_handle_return;
}

CURLMcode curl_multi_add_handle_(CURLM *multi_handle, CURL *curl_handle)
{
    (void)multi_handle;
    (void)curl_handle;
    return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *multi_handle, int *running_handles)
{
    (void)multi_handle;
    *running_handles = 0;
    return CURLM_OK;
}

CURLMcode curl_multi_wait_(CURLM *multi_handle,
                           struct curl_waitfd extra_fds[],
                           unsigned int extra_nfds,
                           int timeout_ms,
                           int *numfds)
{
    (void)multi_handle;
    (void)extra_fds;
    (void)extra_nfds;
    (void)timeout_ms;
    *numfds = 0;
    return CURLM_OK;
}

CURLMcode curl_multi_remove_handle_(CURLM *multi_handle, CURL *curl_handle)
{
    (void)multi_handle;
    (void)curl_handle;
    return CURLM_OK;
}

CURLMcode curl_multi_cleanup_(CURLM *multi_handle)
{
    (void)multi_handle;
    return CURLM_OK;
}

void curl_easy_cleanup(CURL *curl)
{
    (void)curl;
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    mock_curl_return = (CURL *)0x1234;
    mock_perform_return = CURLE_OK;
    mock_multi_handle_return = (CURLM *)0x5678;
    mock_url_encode_return = NULL;
    mock_process_responses_return = NULL;
    mock_http_code = 200;
    mock_response_data = NULL;
    mock_google_search_api_key = talloc_strdup(test_ctx, "test_api_key");
    mock_google_search_engine_id = talloc_strdup(test_ctx, "test_engine_id");
    captured_write_callback = NULL;
    captured_write_data = NULL;
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
    mock_google_search_api_key = NULL;
    mock_google_search_engine_id = NULL;
}

static int32_t run_test(web_search_google_params_t *params)
{
    freopen("/dev/null", "w", stdout);
    int32_t result = web_search_google_execute(test_ctx, params);
    freopen("/dev/tty", "w", stdout);
    return result;
}

static web_search_google_params_t make_params(yyjson_val *allowed, yyjson_val *blocked, size_t allowed_cnt,
                                              size_t blocked_cnt)
{
    web_search_google_params_t params = {
        .query = "test", .num = 10, .start = 1, .allowed_domains_val = allowed, .blocked_domains_val = blocked,
        .allowed_count = allowed_cnt, .blocked_count = blocked_cnt
    };
    return params;
}

static yyjson_val *parse_json(const char *json_str)
{
    char *json = talloc_strdup(test_ctx, json_str);
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(json, strlen(json), 0, &allocator, NULL);
    return yyjson_doc_get_root(doc);
}

START_TEST(test_no_credentials) {
    mock_google_search_api_key = NULL;
    mock_google_search_engine_id = NULL;
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_curl_init_failure) {
    mock_curl_return = NULL;
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_curl_perform_failure) {
    mock_perform_return = CURLE_COULDNT_CONNECT;
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_success_no_domains) {
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_single_allowed_domain) {
    yyjson_val *allowed = parse_json("[\"example.com\"]");
    web_search_google_params_t params = make_params(allowed, NULL, 1, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_single_blocked_domain) {
    yyjson_val *blocked = parse_json("[\"spam.com\"]");
    web_search_google_params_t params = make_params(NULL, blocked, 0, 1);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_multi_allowed_domains) {
    yyjson_val *allowed = parse_json("[\"example.com\", \"test.com\"]");
    web_search_google_params_t params = make_params(allowed, NULL, 2, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_http_400_error) {
    mock_http_code = 400;
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_multi_init_failure) {
    mock_multi_handle_return = NULL;
    yyjson_val *allowed = parse_json("[\"example.com\", \"test.com\"]");
    web_search_google_params_t params = make_params(allowed, NULL, 2, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
START_TEST(test_single_allowed_domain_null) {
    yyjson_val *allowed = parse_json("[null]");
    web_search_google_params_t params = make_params(allowed, NULL, 1, 0);
    run_test(&params);
}
END_TEST

START_TEST(test_single_blocked_domain_null) {
    yyjson_val *blocked = parse_json("[null]");
    web_search_google_params_t params = make_params(NULL, blocked, 0, 1);
    run_test(&params);
}
END_TEST
#endif

START_TEST(test_rate_limit_daily_exceeded) {
    mock_http_code = 429;
    mock_response_data = talloc_strdup(test_ctx, "{\"error\": {\"errors\": [{\"reason\": \"dailyLimitExceeded\"}]}}");
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_rate_limit_quota_exceeded) {
    mock_http_code = 429;
    mock_response_data = talloc_strdup(test_ctx, "{\"error\": {\"errors\": [{\"reason\": \"quotaExceeded\"}]}}");
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_api_error_with_message) {
    mock_http_code = 400;
    mock_response_data = talloc_strdup(test_ctx, "{\"error\": {\"message\": \"Bad Request\"}}");
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_api_error_with_error_message) {
    mock_http_code = 403;
    mock_response_data = talloc_strdup(test_ctx, "{\"error\": {\"errors\": [{\"message\": \"Forbidden\"}]}}");
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_multi_domain_invalid) {
    yyjson_val *allowed = parse_json("[\"example.com\", null, \"test.com\"]");
    web_search_google_params_t params = make_params(allowed, NULL, 3, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

START_TEST(test_http_error_no_details) {
    mock_http_code = 500;
    mock_response_data = talloc_strdup(test_ctx, "{\"other\": \"data\"}");
    web_search_google_params_t params = make_params(NULL, NULL, 0, 0);
    ck_assert_int_eq(run_test(&params), 0);
}
END_TEST

static Suite *web_search_google_direct_suite(void)
{
    Suite *s = suite_create("web_search_google_direct");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_no_credentials);
    tcase_add_test(tc_core, test_curl_init_failure);
    tcase_add_test(tc_core, test_curl_perform_failure);
    tcase_add_test(tc_core, test_success_no_domains);
    tcase_add_test(tc_core, test_single_allowed_domain);
    tcase_add_test(tc_core, test_single_blocked_domain);
    tcase_add_test(tc_core, test_multi_allowed_domains);
    tcase_add_test(tc_core, test_http_400_error);
    tcase_add_test(tc_core, test_multi_init_failure);
#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_single_allowed_domain_null, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_single_blocked_domain_null, SIGABRT);
#endif
    tcase_add_test(tc_core, test_rate_limit_daily_exceeded);
    tcase_add_test(tc_core, test_rate_limit_quota_exceeded);
    tcase_add_test(tc_core, test_api_error_with_message);
    tcase_add_test(tc_core, test_api_error_with_error_message);
    tcase_add_test(tc_core, test_multi_domain_invalid);
    tcase_add_test(tc_core, test_http_error_no_details);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = web_search_google_direct_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
