#include "tests/test_constants.h"
/* Coverage tests for parse_response and google_http_completion_cb */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "apps/ikigai/providers/google/response.h"
#include "apps/ikigai/providers/google/google_internal.h"
#include "apps/ikigai/providers/common/http_multi.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_vtable.h"
#include "shared/error.h"

static TALLOC_CTX *test_ctx;

static bool g_add_request_fail;
static ik_http_completion_t g_mock_http;

/* Captured results from completion callback */
static bool g_cb_called;
static bool g_cb_success;
static int32_t g_cb_http_status;
static ik_error_category_t g_cb_error_category;
static char *g_cb_error_message;
static ik_response_t *g_cb_response;


res_t ik_http_multi_add_request_(void *multi,
                                  const ik_http_request_t *req,
                                  ik_http_write_cb_t write_cb,
                                  void *write_ctx,
                                  ik_http_completion_cb_t completion_cb,
                                  void *completion_ctx)
{
    (void)multi; (void)req; (void)write_cb; (void)write_ctx;

    if (g_add_request_fail) {
        return ERR(test_ctx, IO, "mock add_request failed");
    }

    if (completion_cb != NULL) {
        completion_cb(&g_mock_http, completion_ctx);
    }
    return OK(NULL);
}


static res_t capture_cb(const ik_provider_completion_t *c, void *ctx)
{
    (void)ctx;
    g_cb_called = true;
    g_cb_success = c->success;
    g_cb_http_status = c->http_status;
    g_cb_error_category = c->error_category;
    g_cb_error_message = c->error_message
        ? strdup(c->error_message) : NULL;
    /* Steal response to test_ctx before req_ctx is freed by caller */
    if (c->response) {
        talloc_steal(test_ctx, c->response);
    }
    g_cb_response = c->response;
    return OK(NULL);
}


static ik_google_ctx_t *impl_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    impl_ctx = talloc_zero(test_ctx, ik_google_ctx_t);
    impl_ctx->api_key = talloc_strdup(impl_ctx, "test-key");
    impl_ctx->base_url = talloc_strdup(impl_ctx, "https://generativelanguage.googleapis.com/v1beta");
    impl_ctx->http_multi = NULL;

    g_add_request_fail = false;
    memset(&g_mock_http, 0, sizeof(g_mock_http));
    g_cb_called = false;
    g_cb_success = false;
    g_cb_http_status = 0;
    g_cb_error_category = 0;
    g_cb_error_message = NULL;
    g_cb_response = NULL;
}

static void teardown(void)
{
    free(g_cb_error_message);
    g_cb_error_message = NULL;
    talloc_free(test_ctx);
}

static ik_request_t *make_req(TALLOC_CTX *ctx, const char *model)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(req, model);
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "hi");
    return req;
}

/* ================================================================
 * Tests: successful parse_response paths
 * ================================================================ */

START_TEST(test_success_full_response) {
    const char *body =
        "{\"modelVersion\":\"gemini-2.5-flash\","
        "\"candidates\":[{\"finishReason\":\"STOP\","
        "\"content\":{\"parts\":[{\"text\":\"Hello!\"}]}}],"
        "\"usageMetadata\":{\"promptTokenCount\":5,"
        "\"candidatesTokenCount\":3,\"totalTokenCount\":8}}";

    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    res_t r = ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(!is_err(&r));
    ck_assert(g_cb_called);
    ck_assert(g_cb_success);
    ck_assert_ptr_nonnull(g_cb_response);
    ck_assert_str_eq(g_cb_response->model, "gemini-2.5-flash");
    ck_assert_int_eq(g_cb_response->finish_reason, IK_FINISH_STOP);
    ck_assert_uint_eq(g_cb_response->content_count, (size_t)1);
    ck_assert_int_eq(g_cb_response->usage.input_tokens, 5);
    ck_assert_int_eq(g_cb_response->usage.total_tokens, 8);
}
END_TEST

START_TEST(test_success_null_body) {
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = NULL;
    g_mock_http.response_len = 0;

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    res_t r = ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(!is_err(&r));
    ck_assert(g_cb_called);
    ck_assert(!g_cb_success); /* parse_response returns NULL */
    ck_assert_ptr_null(g_cb_response);
    ck_assert_ptr_nonnull(g_cb_error_message);
}
END_TEST

START_TEST(test_success_invalid_json) {
    const char *body = "not valid json!!!";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    res_t r = ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(!is_err(&r));
    ck_assert(g_cb_called);
    ck_assert(!g_cb_success);
    ck_assert_ptr_null(g_cb_response);
}
END_TEST

START_TEST(test_success_non_obj_root) {
    const char *body = "[1,2,3]";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(g_cb_called);
    ck_assert(!g_cb_success);
}
END_TEST

START_TEST(test_success_no_text_in_part) {
    /* Part without "text" field - content_count stays 0 */
    const char *body =
        "{\"candidates\":[{\"finishReason\":\"STOP\","
        "\"content\":{\"parts\":[{\"functionCall\":{}}]}}]}";

    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(g_cb_success);
    ck_assert_uint_eq(g_cb_response->content_count, (size_t)0);
}
END_TEST

START_TEST(test_success_no_content_obj) {
    /* candidate has no "content" - returns early from parse_candidate */
    const char *body =
        "{\"candidates\":[{\"finishReason\":\"STOP\"}]}";

    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(g_cb_success);
    ck_assert_int_eq(g_cb_response->finish_reason, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_success_finish_reason_not_string) {
    /* finishReason is int - maps to IK_FINISH_UNKNOWN */
    const char *body =
        "{\"candidates\":[{\"finishReason\":42,"
        "\"content\":{\"parts\":[{\"text\":\"hi\"}]}}]}";

    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(g_cb_success);
    ck_assert_int_eq(g_cb_response->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

/* ================================================================
 * Tests: HTTP error paths
 * ================================================================ */

START_TEST(test_http_error_401) {
    const char *body = "{\"error\":{\"code\":401,\"message\":\"Unauthorized\"}}";
    g_mock_http.type = IK_HTTP_CLIENT_ERROR;
    g_mock_http.http_code = 401;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    res_t r = ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(!is_err(&r));
    ck_assert(g_cb_called);
    ck_assert(!g_cb_success);
}
END_TEST

START_TEST(test_http_error_429) {
    const char *body = "{\"error\":{\"code\":429,\"message\":\"Rate limited\"}}";
    g_mock_http.type = IK_HTTP_CLIENT_ERROR;
    g_mock_http.http_code = 429;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(!g_cb_success);
    ck_assert_int_eq(g_cb_error_category, IK_ERR_CAT_RATE_LIMIT);
}
END_TEST

START_TEST(test_add_request_failure) {
    g_add_request_fail = true;

    ik_request_t *req = make_req(test_ctx, "gemini-2.5-flash");
    res_t r = ik_google_start_request(impl_ctx, req, capture_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert(!g_cb_called);
    talloc_free(r.err);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_response_http_suite(void)
{
    Suite *s = suite_create("Google response HTTP");

    TCase *tc_success = tcase_create("success");
    tcase_set_timeout(tc_success, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_success, setup, teardown);
    tcase_add_test(tc_success, test_success_full_response);
    tcase_add_test(tc_success, test_success_null_body);
    tcase_add_test(tc_success, test_success_invalid_json);
    tcase_add_test(tc_success, test_success_non_obj_root);
    tcase_add_test(tc_success, test_success_no_text_in_part);
    tcase_add_test(tc_success, test_success_no_content_obj);
    tcase_add_test(tc_success, test_success_finish_reason_not_string);
    suite_add_tcase(s, tc_success);

    TCase *tc_error = tcase_create("http_error");
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_http_error_401);
    tcase_add_test(tc_error, test_http_error_429);
    tcase_add_test(tc_error, test_add_request_failure);
    suite_add_tcase(s, tc_error);

    return s;
}

int main(void)
{
    Suite *s = google_response_http_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/providers/google/google_response_http_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
