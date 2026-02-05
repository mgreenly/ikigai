#include "apps/ikigai/providers/common/http_multi_info.c"
#include "apps/ikigai/providers/common/http_multi_internal.h"
#include "shared/wrapper.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <curl/curl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/**
 * Unit tests for http_multi_info.c
 *
 * Tests ik_http_multi_info_read() with various completion scenarios.
 */

/* Test context */
static TALLOC_CTX *test_ctx = NULL;

/* Setup/teardown */
static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

/**
 * Mock data for curl_multi_info_read
 */
static CURLMsg *g_mock_msg = NULL;
static int g_mock_msgs_left = 0;
static bool g_mock_info_read_return_null = false;
static int g_info_read_call_count = 0;

/* Mock curl_multi_info_read to return controlled messages */
CURLMsg *curl_multi_info_read_(CURLM *multi_handle, int *msgs_in_queue)
{
    (void)multi_handle;
    g_info_read_call_count++;

    if (g_mock_info_read_return_null || g_info_read_call_count > 1) {
        *msgs_in_queue = 0;
        return NULL;
    }

    *msgs_in_queue = g_mock_msgs_left;
    return g_mock_msg;
}

/* Mock curl_easy_getinfo for response code */
static long g_mock_response_code = 200;

CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...)
{
    (void)curl;
    va_list args;
    va_start(args, info);

    if (info == CURLINFO_RESPONSE_CODE) {
        long *out = va_arg(args, long *);
        *out = g_mock_response_code;
    }

    va_end(args);
    return CURLE_OK;
}

/* Mock curl_easy_strerror */
const char *curl_easy_strerror_(CURLcode code)
{
    (void)code;
    return "Mock error message";
}

/* Mock curl cleanup functions */
CURLMcode curl_multi_remove_handle_(CURLM *multi_handle, CURL *easy_handle)
{
    (void)multi_handle;
    (void)easy_handle;
    return CURLM_OK;
}

void curl_easy_cleanup_(CURL *handle)
{
    if (handle != NULL) {
        curl_easy_cleanup(handle);
    }
}

void curl_slist_free_all_(struct curl_slist *list)
{
    (void)list;
}

/* Track completion callbacks */
static int g_completion_callback_count = 0;
static ik_http_completion_t g_last_completion = {0};

static void test_completion_callback(const ik_http_completion_t *completion, void *ctx)
{
    (void)ctx;
    g_completion_callback_count++;
    g_last_completion = *completion;
}

/* Reset mock state */
static void reset_mocks(void)
{
    g_mock_msg = NULL;
    g_mock_msgs_left = 0;
    g_mock_info_read_return_null = false;
    g_info_read_call_count = 0;
    g_mock_response_code = 200;
    g_completion_callback_count = 0;
    memset(&g_last_completion, 0, sizeof(g_last_completion));
}

/* Helper to create a mock active request */
static active_request_t *create_mock_request(void *parent, CURL *easy, ik_http_completion_cb_t cb)
{
    active_request_t *req = talloc_zero(parent, active_request_t);
    req->easy_handle = easy;
    req->headers = NULL;
    req->completion_cb = cb;
    req->completion_ctx = NULL;
    req->write_ctx = talloc_zero(req, http_write_ctx_t);
    req->write_ctx->response_buffer = NULL;
    req->write_ctx->response_len = 0;
    req->write_ctx->buffer_capacity = 0;
    return req;
}

/* Helper to setup a basic test scenario */
static ik_http_multi_t *setup_multi_with_request(TALLOC_CTX *ctx, CURL *easy,
                                                 ik_http_completion_cb_t cb,
                                                 active_request_t **out_req)
{
    res_t res = ik_http_multi_create(ctx);
    ck_assert(!res.is_err);
    ik_http_multi_t *multi = res.ok;

    active_request_t *req = create_mock_request(multi, easy, cb);
    multi->active_requests = talloc_array(multi, active_request_t *, 1);
    multi->active_requests[0] = req;
    multi->active_count = 1;

    if (out_req != NULL) {
        *out_req = req;
    }
    return multi;
}

/* Helper to setup mock message */
static void setup_mock_message(CURLMsg *msg, CURL *easy, CURLcode result, long http_code)
{
    msg->msg = CURLMSG_DONE;
    msg->easy_handle = easy;
    msg->data.result = result;
    g_mock_msg = msg;
    g_mock_msgs_left = 0;
    g_mock_response_code = http_code;
}

/**
 * Test: HTTP 200 success response
 * Covers: line 18 (msg != NULL), line 23/24 (found), line 33 (CURLE_OK), line 40 (200-299)
 */
START_TEST(test_info_read_http_200_success) {
    reset_mocks();
    CURL *easy = curl_easy_init();
    ck_assert_ptr_nonnull(easy);

    active_request_t *req;
    ik_http_multi_t *multi = setup_multi_with_request(test_ctx, easy, test_completion_callback, &req);
    req->write_ctx->response_buffer = talloc_strdup(req->write_ctx, "test response");
    req->write_ctx->response_len = strlen("test response");
    req->write_ctx->buffer_capacity = 100;

    CURLMsg msg = {0};
    setup_mock_message(&msg, easy, CURLE_OK, 200);
    ik_http_multi_info_read(multi, NULL);

    ck_assert_int_eq(g_completion_callback_count, 1);
    ck_assert_int_eq(g_last_completion.type, IK_HTTP_SUCCESS);
    ck_assert_int_eq(g_last_completion.http_code, 200);
    ck_assert_int_eq(g_last_completion.curl_code, CURLE_OK);
    ck_assert_uint_eq(multi->active_count, 0);

    talloc_free(multi);
}

END_TEST

/* Compact test helper for HTTP status code tests */
static void test_http_status(long code, ik_http_status_type_t expected_type)
{
    reset_mocks();
    CURL *easy = curl_easy_init();
    ck_assert_ptr_nonnull(easy);
    ik_http_multi_t *multi = setup_multi_with_request(test_ctx, easy, test_completion_callback, NULL);

    CURLMsg msg = {0};
    setup_mock_message(&msg, easy, CURLE_OK, code);
    ik_http_multi_info_read(multi, NULL);

    ck_assert_int_eq(g_completion_callback_count, 1);
    ck_assert_int_eq(g_last_completion.type, expected_type);
    ck_assert_int_eq(g_last_completion.http_code, code);
    talloc_free(multi);
}

START_TEST(test_info_read_http_404_client_error) {
    test_http_status(404, IK_HTTP_CLIENT_ERROR);
}
END_TEST

START_TEST(test_info_read_http_500_server_error) {
    test_http_status(503, IK_HTTP_SERVER_ERROR);
}
END_TEST

START_TEST(test_info_read_http_100_unexpected) {
    test_http_status(100, IK_HTTP_NETWORK_ERROR);
}
END_TEST

START_TEST(test_info_read_http_300_unexpected) {
    test_http_status(301, IK_HTTP_NETWORK_ERROR);
}
END_TEST

START_TEST(test_info_read_http_600_unexpected) {
    test_http_status(600, IK_HTTP_NETWORK_ERROR);
}
END_TEST

START_TEST(test_info_read_network_error) {
    reset_mocks();
    CURL *easy = curl_easy_init();
    ck_assert_ptr_nonnull(easy);
    ik_http_multi_t *multi = setup_multi_with_request(test_ctx, easy, test_completion_callback, NULL);

    CURLMsg msg = {0};
    setup_mock_message(&msg, easy, CURLE_COULDNT_CONNECT, 0);
    ik_http_multi_info_read(multi, NULL);

    ck_assert_int_eq(g_completion_callback_count, 1);
    ck_assert_int_eq(g_last_completion.type, IK_HTTP_NETWORK_ERROR);
    ck_assert_int_eq(g_last_completion.http_code, 0);
    ck_assert_int_eq(g_last_completion.curl_code, CURLE_COULDNT_CONNECT);
    talloc_free(multi);
}
END_TEST

START_TEST(test_info_read_no_completion_callback) {
    reset_mocks();
    CURL *easy = curl_easy_init();
    ck_assert_ptr_nonnull(easy);
    ik_http_multi_t *multi = setup_multi_with_request(test_ctx, easy, NULL, NULL);

    CURLMsg msg = {0};
    setup_mock_message(&msg, easy, CURLE_OK, 200);
    ik_http_multi_info_read(multi, NULL);

    ck_assert_int_eq(g_completion_callback_count, 0);
    ck_assert_uint_eq(multi->active_count, 0);
    talloc_free(multi);
}
END_TEST

START_TEST(test_info_read_non_done_message) {
    reset_mocks();
    CURL *easy = curl_easy_init();
    ck_assert_ptr_nonnull(easy);
    ik_http_multi_t *multi = setup_multi_with_request(test_ctx, easy, test_completion_callback, NULL);

    CURLMsg msg = {0};
    msg.msg = CURLMSG_NONE;
    msg.easy_handle = easy;
    msg.data.result = CURLE_OK;
    g_mock_msg = &msg;

    ik_http_multi_info_read(multi, NULL);

    ck_assert_int_eq(g_completion_callback_count, 0);
    ck_assert_uint_eq(multi->active_count, 1);
    talloc_free(multi);
}
END_TEST

START_TEST(test_info_read_handle_not_found) {
    reset_mocks();
    CURL *easy1 = curl_easy_init();
    CURL *easy2 = curl_easy_init();
    ck_assert_ptr_nonnull(easy1);
    ck_assert_ptr_nonnull(easy2);

    ik_http_multi_t *multi = setup_multi_with_request(test_ctx, easy1, test_completion_callback, NULL);

    CURLMsg msg = {0};
    setup_mock_message(&msg, easy2, CURLE_OK, 200);      /* Different handle */
    ik_http_multi_info_read(multi, NULL);

    ck_assert_int_eq(g_completion_callback_count, 0);
    ck_assert_uint_eq(multi->active_count, 1);

    curl_easy_cleanup(easy2);
    talloc_free(multi);
}

END_TEST

START_TEST(test_info_read_remove_middle_element) {
    reset_mocks();
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);
    ik_http_multi_t *multi = res.ok;

    CURL *easy1 = curl_easy_init();
    CURL *easy2 = curl_easy_init();
    CURL *easy3 = curl_easy_init();

    active_request_t *req1 = create_mock_request(multi, easy1, NULL);
    active_request_t *req2 = create_mock_request(multi, easy2, test_completion_callback);
    active_request_t *req3 = create_mock_request(multi, easy3, NULL);

    multi->active_requests = talloc_array(multi, active_request_t *, 3);
    multi->active_requests[0] = req1;
    multi->active_requests[1] = req2;
    multi->active_requests[2] = req3;
    multi->active_count = 3;

    CURLMsg msg = {0};
    setup_mock_message(&msg, easy2, CURLE_OK, 200);
    ik_http_multi_info_read(multi, NULL);

    ck_assert_int_eq(g_completion_callback_count, 1);
    ck_assert_uint_eq(multi->active_count, 2);
    ck_assert_ptr_eq(multi->active_requests[0], req1);
    ck_assert_ptr_eq(multi->active_requests[1], req3);

    talloc_free(multi);
}

END_TEST

/**
 * Test Suite Configuration
 */

static Suite *http_multi_info_suite(void)
{
    Suite *s = suite_create("HTTP Multi Info Read");

    /* Completion scenario tests */
    TCase *tc_completion = tcase_create("Completion Scenarios");
    tcase_set_timeout(tc_completion, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_completion, setup, teardown);
    tcase_add_test(tc_completion, test_info_read_http_200_success);
    tcase_add_test(tc_completion, test_info_read_http_404_client_error);
    tcase_add_test(tc_completion, test_info_read_http_500_server_error);
    tcase_add_test(tc_completion, test_info_read_http_100_unexpected);
    tcase_add_test(tc_completion, test_info_read_http_300_unexpected);
    tcase_add_test(tc_completion, test_info_read_http_600_unexpected);
    tcase_add_test(tc_completion, test_info_read_network_error);
    tcase_add_test(tc_completion, test_info_read_no_completion_callback);
    tcase_add_test(tc_completion, test_info_read_non_done_message);
    tcase_add_test(tc_completion, test_info_read_handle_not_found);
    tcase_add_test(tc_completion, test_info_read_remove_middle_element);
    suite_add_tcase(s, tc_completion);

    return s;
}

int main(void)
{
    Suite *s = http_multi_info_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/common/http_multi_info_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
