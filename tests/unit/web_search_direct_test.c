#include "tools/web_search/web_search.h"

#include "json_allocator.h"
#include "panic.h"
#include "wrapper_posix.h"
#include "wrapper_stdlib.h"
#include "wrapper_web.h"

#include <check.h>
#include <curl/curl.h>
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
static int64_t mock_http_code = 200;
static char *mock_response_data = NULL;
static char *mock_env_brave_key = NULL;
static char *mock_env_home = NULL;

// Captured from curl_easy_setopt_
static size_t (*captured_write_callback)(void *, size_t, size_t, void *) = NULL;
static void *captured_write_data = NULL;

CURL *curl_easy_init_(void)
{
    return mock_curl_return;
}

CURLcode curl_easy_perform_(CURL *curl)
{
    (void)curl;
    if (mock_perform_return == CURLE_OK && captured_write_callback != NULL && mock_response_data != NULL) {
        size_t len = strlen(mock_response_data);
        captured_write_callback((void *)mock_response_data, 1, len, captured_write_data);
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

char *curl_easy_escape(CURL *curl, const char *string, int length)
{
    (void)curl;
    (void)length;
    return strdup(string ? string : "");
}

void curl_free(void *ptr)
{
    free(ptr);
}

struct curl_slist *curl_slist_append(struct curl_slist *list, const char *string)
{
    (void)string;
    return list;
}

void curl_slist_free_all(struct curl_slist *list)
{
    (void)list;
}

char *getenv_(const char *name)
{
    if (strcmp(name, "BRAVE_API_KEY") == 0) {
        return mock_env_brave_key;
    }
    if (strcmp(name, "HOME") == 0) {
        return mock_env_home;
    }
    if (strcmp(name, "IKIGAI_CONFIG_DIR") == 0) {
        return NULL;
    }
    return NULL;
}

// Forward declare libxml2 types
typedef struct _xmlDoc *htmlDocPtr;
typedef struct _xmlDoc *xmlDocPtr;

htmlDocPtr htmlReadMemory_(const char *buffer, int size, const char *URL, const char *encoding, int options)
{
    (void)buffer;
    (void)size;
    (void)URL;
    (void)encoding;
    (void)options;
    return NULL;
}

void xmlFreeDoc_(xmlDocPtr cur)
{
    (void)cur;
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    mock_curl_return = (CURL *)0x1234;
    mock_perform_return = CURLE_OK;
    mock_http_code = 200;
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": []}}");
    mock_env_brave_key = talloc_strdup(test_ctx, "test_key");
    mock_env_home = talloc_strdup(test_ctx, "/tmp");
    captured_write_callback = NULL;
    captured_write_data = NULL;
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
    mock_env_brave_key = NULL;
    mock_env_home = NULL;
    captured_write_callback = NULL;
    captured_write_data = NULL;
}

static web_search_params_t mk_p(void)
{
    web_search_params_t p = {.query = "test", .count = 10, .offset = 0, .allowed_domains = NULL, .blocked_domains = NULL};
    return p;
}

static int32_t run(web_search_params_t *p)
{
    freopen("/dev/null", "w", stdout);
    int32_t r = web_search_execute(test_ctx, p);
    freopen("/dev/tty", "w", stdout);
    return r;
}

static yyjson_val *mk_f(const char *j)
{
    char *s = talloc_strdup(test_ctx, j);
    yyjson_alc a = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *d = yyjson_read_opts(s, strlen(s), 0, &a, NULL);
    return yyjson_doc_get_root(d);
}

START_TEST(test_curl_init_failure)
{
    mock_curl_return = NULL;
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_curl_perform_failure)
{
    mock_perform_return = CURLE_COULDNT_CONNECT;
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_no_api_key)
{
    mock_env_brave_key = NULL;
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_http_401_error)
{
    mock_http_code = 401;
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_http_429_rate_limit)
{
    mock_http_code = 429;
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_http_500_error)
{
    mock_http_code = 500;
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_success_with_results)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com\", \"title\": \"Test\", \"description\": \"Test desc\"}]}}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_success_empty_results)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": []}}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_invalid_json_response)
{
    mock_response_data = talloc_strdup(test_ctx, "not json");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_missing_web_field)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"results\": []}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_empty_response_data)
{
    mock_response_data = NULL;
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_results_not_array)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": \"not an array\"}}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_http_403_error)
{
    mock_http_code = 403;
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_invalid_url_entry)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"title\": \"No URL\"}]}}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_allowed_domains_match)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com/page\", \"title\": \"Test\"}]}}");
    web_search_params_t p = mk_p();
    p.allowed_domains = mk_f("[\"example.com\"]");
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_allowed_domains_no_match)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com/page\", \"title\": \"Test\"}]}}");
    web_search_params_t p = mk_p();
    p.allowed_domains = mk_f("[\"different.com\"]");
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_blocked_domains_match)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com/page\", \"title\": \"Test\"}]}}");
    web_search_params_t p = mk_p();
    p.blocked_domains = mk_f("[\"example.com\"]");
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_blocked_domains_no_match)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com/page\", \"title\": \"Test\"}]}}");
    web_search_params_t p = mk_p();
    p.blocked_domains = mk_f("[\"different.com\"]");
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_empty_api_key_env)
{
    mock_env_brave_key = talloc_strdup(test_ctx, "");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_result_missing_title)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com\", \"description\": \"Test desc\"}]}}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_result_missing_description)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com\", \"title\": \"Test\"}]}}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_result_title_not_string)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com\", \"title\": 123}]}}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_result_description_not_string)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com\", \"title\": \"Test\", \"description\": false}]}}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_result_url_not_string)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": 456, \"title\": \"Test\"}]}}");
    web_search_params_t p = mk_p();
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_allowed_domains_non_string)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com/page\", \"title\": \"Test\"}]}}");
    web_search_params_t p = mk_p();
    p.allowed_domains = mk_f("[123, \"example.com\"]");
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

START_TEST(test_blocked_domains_non_string)
{
    mock_response_data = talloc_strdup(test_ctx, "{\"web\": {\"results\": [{\"url\": \"https://example.com/page\", \"title\": \"Test\"}]}}");
    web_search_params_t p = mk_p();
    p.blocked_domains = mk_f("[456]");
    ck_assert_int_eq(run(&p), 0);
}
END_TEST

static Suite *web_search_direct_suite(void)
{
    Suite *s = suite_create("web_search_direct");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_curl_init_failure);
    tcase_add_test(tc_core, test_curl_perform_failure);
    tcase_add_test(tc_core, test_no_api_key);
    tcase_add_test(tc_core, test_http_401_error);
    tcase_add_test(tc_core, test_http_429_rate_limit);
    tcase_add_test(tc_core, test_http_500_error);
    tcase_add_test(tc_core, test_success_with_results);
    tcase_add_test(tc_core, test_success_empty_results);
    tcase_add_test(tc_core, test_invalid_json_response);
    tcase_add_test(tc_core, test_missing_web_field);
    tcase_add_test(tc_core, test_empty_response_data);
    tcase_add_test(tc_core, test_results_not_array);
    tcase_add_test(tc_core, test_http_403_error);
    tcase_add_test(tc_core, test_invalid_url_entry);
    tcase_add_test(tc_core, test_allowed_domains_match);
    tcase_add_test(tc_core, test_allowed_domains_no_match);
    tcase_add_test(tc_core, test_blocked_domains_match);
    tcase_add_test(tc_core, test_blocked_domains_no_match);
    tcase_add_test(tc_core, test_empty_api_key_env);
    tcase_add_test(tc_core, test_result_missing_title);
    tcase_add_test(tc_core, test_result_missing_description);
    tcase_add_test(tc_core, test_result_title_not_string);
    tcase_add_test(tc_core, test_result_description_not_string);
    tcase_add_test(tc_core, test_result_url_not_string);
    tcase_add_test(tc_core, test_allowed_domains_non_string);
    tcase_add_test(tc_core, test_blocked_domains_non_string);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = web_search_direct_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/web_search_direct_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
