/**
 * @file web_fetch_direct_test.c
 * @brief Direct unit tests for web_fetch logic with mocking
 */
#include "tests/test_constants.h"

#include "tools/web_fetch/web_fetch.h"

#include <check.h>
#include <curl/curl.h>
#include <inttypes.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "shared/json_allocator.h"
#include "shared/panic.h"

// Forward declarations for wrapper functions we'll define
CURL *curl_easy_init_(void);
CURLcode curl_easy_perform_(CURL *curl);
void curl_easy_cleanup_(CURL *curl);
CURLcode curl_easy_setopt_(CURL *curl, int option, ...);
CURLcode curl_easy_getinfo_(CURL *curl, int info, ...);
const char *curl_easy_strerror_(CURLcode errornum);
htmlDocPtr htmlReadMemory_(const char *buffer, int size, const char *URL, const char *encoding, int options);
void xmlFreeDoc_(xmlDocPtr cur);

static TALLOC_CTX *test_ctx;

// Mock control variables
static bool mock_curl_init_should_fail = false;
static CURLcode mock_curl_perform_return = CURLE_OK;
static htmlDocPtr mock_htmlReadMemory_return = NULL;
static bool mock_should_call_write_callback = false;
static const char *mock_response_data = NULL;
static int64_t mock_http_code = 200;

// Storage for setopt parameters
static size_t (*write_function_callback)(void *, size_t, size_t, void *) = NULL;
static void *write_data_ptr = NULL;

CURL *curl_easy_init_(void)
{
    if (mock_curl_init_should_fail) {
        return NULL;
    }
    return curl_easy_init();
}

CURLcode curl_easy_perform_(CURL *curl)
{
    (void)curl;
    if (mock_curl_perform_return == CURLE_OK && mock_should_call_write_callback && write_function_callback &&
        mock_response_data) {
        size_t len = strlen(mock_response_data);
        // Need to allocate a non-const buffer for the callback
        char *buffer = talloc_array(test_ctx, char, (unsigned int)(len + 1));
        if (buffer != NULL) { // LCOV_EXCL_BR_LINE
            memcpy(buffer, mock_response_data, len);
            buffer[len] = '\0';
            write_function_callback(buffer, 1, len, write_data_ptr);
        }
    }
    return mock_curl_perform_return;
}

void curl_easy_cleanup_(CURL *curl)
{
    curl_easy_cleanup(curl);
}

CURLcode curl_easy_setopt_(CURL *curl, int option, ...)
{
    (void)curl;
    va_list args;
    va_start(args, option);

    if (option == CURLOPT_WRITEFUNCTION) {
        write_function_callback = va_arg(args, size_t (*)(void *, size_t, size_t, void *));
    } else if (option == CURLOPT_WRITEDATA) {
        write_data_ptr = va_arg(args, void *);
    }
    // Ignore other options

    va_end(args);
    return CURLE_OK;
}

CURLcode curl_easy_getinfo_(CURL *curl, int info, ...)
{
    (void)curl;
    va_list args;
    va_start(args, info);

    if (info == CURLINFO_RESPONSE_CODE) {
        int64_t *code_ptr = va_arg(args, int64_t *);
        *code_ptr = mock_http_code;
    } else if (info == CURLINFO_EFFECTIVE_URL) {
        char **url_ptr = va_arg(args, char **);
        *url_ptr = NULL;
    }

    va_end(args);
    return CURLE_OK;
}

const char *curl_easy_strerror_(CURLcode errornum)
{
    return curl_easy_strerror(errornum);
}

htmlDocPtr htmlReadMemory_(const char *buffer, int size, const char *URL, const char *encoding, int options)
{
    (void)buffer;
    (void)size;
    (void)URL;
    (void)encoding;
    (void)options;
    return mock_htmlReadMemory_return;
}

void xmlFreeDoc_(xmlDocPtr cur)
{
    (void)cur;
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    // Reset mocks to default success values
    mock_curl_init_should_fail = false;
    mock_curl_perform_return = CURLE_OK;
    mock_htmlReadMemory_return = NULL;
    mock_should_call_write_callback = false;
    mock_response_data = NULL;
    mock_http_code = 200;
    write_function_callback = NULL;
    write_data_ptr = NULL;
}

static void teardown(void)
{
    if (mock_htmlReadMemory_return != NULL) {
        xmlFreeDoc(mock_htmlReadMemory_return);
        mock_htmlReadMemory_return = NULL;
    }
    talloc_free(test_ctx);
}

START_TEST(test_curl_init_failure) {
    mock_curl_init_should_fail = true;

    web_fetch_params_t params = {
        .url = "http://example.com",
        .has_offset = false,
        .has_limit = false
    };

    FILE *original_stdout = stdout;
    char output_buffer[4096] = {0};
    FILE *mock_stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");
    ck_assert_ptr_nonnull(mock_stdout);
    stdout = mock_stdout;

    int32_t result = web_fetch_execute(test_ctx, &params);
    fflush(stdout);

    fclose(mock_stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output_buffer, "\"error\""));
    ck_assert_ptr_nonnull(strstr(output_buffer, "Failed to initialize HTTP client"));
}
END_TEST

START_TEST(test_curl_perform_failure) {
    mock_curl_perform_return = CURLE_COULDNT_CONNECT;

    web_fetch_params_t params = {
        .url = "http://example.com",
        .has_offset = false,
        .has_limit = false
    };

    FILE *original_stdout = stdout;
    char output_buffer[4096] = {0};
    FILE *mock_stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");
    ck_assert_ptr_nonnull(mock_stdout);
    stdout = mock_stdout;

    int32_t result = web_fetch_execute(test_ctx, &params);
    fflush(stdout);

    fclose(mock_stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output_buffer, "\"error\""));
    ck_assert_ptr_nonnull(strstr(output_buffer, "Failed to fetch URL"));
}
END_TEST

START_TEST(test_parse_failure) {
    mock_htmlReadMemory_return = NULL;

    web_fetch_params_t params = {
        .url = "http://example.com",
        .has_offset = false,
        .has_limit = false
    };

    FILE *original_stdout = stdout;
    char output_buffer[4096] = {0};
    FILE *mock_stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");
    ck_assert_ptr_nonnull(mock_stdout);
    stdout = mock_stdout;

    int32_t result = web_fetch_execute(test_ctx, &params);
    fflush(stdout);

    fclose(mock_stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output_buffer, "\"error\""));
    ck_assert_ptr_nonnull(strstr(output_buffer, "Failed to parse HTML"));
}
END_TEST

START_TEST(test_http_error) {
    const char *html_response = "<html><body>Not Found</body></html>";
    mock_response_data = html_response;
    mock_should_call_write_callback = true;
    mock_http_code = 404;

    web_fetch_params_t params = {
        .url = "http://example.com",
        .has_offset = false,
        .has_limit = false
    };

    FILE *original_stdout = stdout;
    char output_buffer[4096] = {0};
    FILE *mock_stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");
    ck_assert_ptr_nonnull(mock_stdout);
    stdout = mock_stdout;

    int32_t result = web_fetch_execute(test_ctx, &params);
    fflush(stdout);

    fclose(mock_stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output_buffer, "\"error\""));
    ck_assert_ptr_nonnull(strstr(output_buffer, "HTTP 404 error"));
}
END_TEST

START_TEST(test_success_with_html) {
    const char *html_response = "<html><head><title>Test Page</title></head><body><p>Hello World</p></body></html>";
    mock_response_data = html_response;
    mock_should_call_write_callback = true;
    mock_htmlReadMemory_return = htmlReadMemory(html_response,
                                                (int)strlen(html_response),
                                                NULL,
                                                NULL,
                                                HTML_PARSE_NOERROR);

    web_fetch_params_t params = {
        .url = "http://example.com",
        .has_offset = false,
        .has_limit = false
    };

    FILE *original_stdout = stdout;
    char output_buffer[8192] = {0};
    FILE *mock_stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");
    ck_assert_ptr_nonnull(mock_stdout);
    stdout = mock_stdout;

    int32_t result = web_fetch_execute(test_ctx, &params);
    fflush(stdout);

    fclose(mock_stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output_buffer, "\"url\""));
    ck_assert_ptr_nonnull(strstr(output_buffer, "\"title\""));
    ck_assert_ptr_nonnull(strstr(output_buffer, "\"content\""));
    ck_assert_ptr_nonnull(strstr(output_buffer, "Test Page"));
}
END_TEST

START_TEST(test_success_with_offset) {
    const char *html_response = "<html><body><p>Line1</p><p>Line2</p><p>Line3</p></body></html>";
    mock_response_data = html_response;
    mock_should_call_write_callback = true;
    mock_htmlReadMemory_return = htmlReadMemory(html_response,
                                                (int)strlen(html_response),
                                                NULL,
                                                NULL,
                                                HTML_PARSE_NOERROR);

    web_fetch_params_t params = {
        .url = "http://example.com",
        .has_offset = true,
        .offset = 2,
        .has_limit = false
    };

    FILE *original_stdout = stdout;
    char output_buffer[8192] = {0};
    FILE *mock_stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");
    ck_assert_ptr_nonnull(mock_stdout);
    stdout = mock_stdout;

    int32_t result = web_fetch_execute(test_ctx, &params);
    fflush(stdout);

    fclose(mock_stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output_buffer, "\"content\""));
}
END_TEST

START_TEST(test_success_with_limit) {
    const char *html_response = "<html><body><p>Line1</p><p>Line2</p><p>Line3</p></body></html>";
    mock_response_data = html_response;
    mock_should_call_write_callback = true;
    mock_htmlReadMemory_return = htmlReadMemory(html_response,
                                                (int)strlen(html_response),
                                                NULL,
                                                NULL,
                                                HTML_PARSE_NOERROR);

    web_fetch_params_t params = {
        .url = "http://example.com",
        .has_offset = false,
        .has_limit = true,
        .limit = 2
    };

    FILE *original_stdout = stdout;
    char output_buffer[8192] = {0};
    FILE *mock_stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");
    ck_assert_ptr_nonnull(mock_stdout);
    stdout = mock_stdout;

    int32_t result = web_fetch_execute(test_ctx, &params);
    fflush(stdout);

    fclose(mock_stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output_buffer, "\"content\""));
}
END_TEST

static Suite *web_fetch_suite(void)
{
    Suite *s = suite_create("web_fetch");
    TCase *tc = tcase_create("core");

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_curl_init_failure);
    tcase_add_test(tc, test_curl_perform_failure);
    tcase_add_test(tc, test_parse_failure);
    tcase_add_test(tc, test_http_error);
    tcase_add_test(tc, test_success_with_html);
    tcase_add_test(tc, test_success_with_offset);
    tcase_add_test(tc, test_success_with_limit);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = web_fetch_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/web_fetch/web_fetch_direct_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
