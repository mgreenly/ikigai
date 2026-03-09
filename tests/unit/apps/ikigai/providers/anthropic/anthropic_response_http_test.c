#include "tests/test_constants.h"
/* Coverage tests for parse_response and anthropic_http_completion_cb */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "apps/ikigai/providers/anthropic/response.h"
#include "apps/ikigai/providers/anthropic/anthropic_internal.h"
#include "apps/ikigai/providers/common/http_multi.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_vtable.h"
#include "shared/error.h"

static TALLOC_CTX *test_ctx; /* forward decl — defined in Fixtures */

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

    /* Invoke the completion callback synchronously */
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
    /* error_message owned by req_ctx which gets freed; copy it */
    g_cb_error_message = c->error_message
        ? strdup(c->error_message) : NULL;
    /* Steal response to test_ctx before req_ctx is freed by caller */
    if (c->response) {
        talloc_steal(test_ctx, c->response);
    }
    g_cb_response = c->response;
    return OK(NULL);
}


static ik_anthropic_ctx_t *impl_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    impl_ctx = talloc_zero(test_ctx, ik_anthropic_ctx_t);
    impl_ctx->api_key = talloc_strdup(impl_ctx, "test-key");
    impl_ctx->base_url = talloc_strdup(impl_ctx, "https://api.anthropic.com");
    impl_ctx->http_multi = NULL; /* not reached — mocked */

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

static ik_request_t make_req(const char *model)
{
    ik_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = (char *)(uintptr_t)model;
    return req;
}

/* Tests: parse_response */
START_TEST(test_success_full_response) {
    /* Full JSON: model, stop_reason, usage, text content */
    const char *body =
        "{\"model\":\"claude-3\","
        "\"stop_reason\":\"end_turn\","
        "\"usage\":{\"input_tokens\":10,\"output_tokens\":20},"
        "\"content\":[{\"type\":\"text\",\"text\":\"Hello\"}]}";

    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t req = make_req("claude-3");
    res_t r = ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!is_err(&r));
    ck_assert(g_cb_called);
    ck_assert(g_cb_success);
    ck_assert_ptr_nonnull(g_cb_response);
    ck_assert_str_eq(g_cb_response->model, "claude-3");
    ck_assert_int_eq(g_cb_response->usage.input_tokens, 10);
    ck_assert_int_eq(g_cb_response->usage.output_tokens, 20);
    ck_assert_int_eq(g_cb_response->usage.total_tokens, 30);
    ck_assert_uint_eq(g_cb_response->content_count, (size_t)1);
    ck_assert_int_eq(g_cb_response->finish_reason, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_success_no_model_field) {
    /* JSON missing model field — model stays NULL */
    const char *body = "{\"stop_reason\":\"max_tokens\"}";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t req = make_req("claude-3");
    res_t r = ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!is_err(&r));
    ck_assert(g_cb_called);
    ck_assert(g_cb_success);
    ck_assert_ptr_null(g_cb_response->model);
    ck_assert_int_eq(g_cb_response->finish_reason, IK_FINISH_LENGTH);
}
END_TEST

START_TEST(test_success_stop_reason_not_string) {
    /* stop_reason is int — goes to else branch (IK_FINISH_UNKNOWN) */
    const char *body = "{\"model\":\"m\",\"stop_reason\":42}";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(g_cb_success);
    ck_assert_int_eq(g_cb_response->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_success_non_text_content_block) {
    /* Content block with type != "text" — skipped, content_count stays 0 */
    const char *body =
        "{\"model\":\"m\","
        "\"content\":[{\"type\":\"tool_use\",\"id\":\"t1\"}]}";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(g_cb_success);
    ck_assert_uint_eq(g_cb_response->content_count, (size_t)0);
}
END_TEST

START_TEST(test_success_content_block_no_type) {
    /* Content block without "type" field — skipped */
    const char *body =
        "{\"model\":\"m\","
        "\"content\":[{\"text\":\"hi\"}]}";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(g_cb_success);
    ck_assert_uint_eq(g_cb_response->content_count, (size_t)0);
}
END_TEST

START_TEST(test_success_text_null_field) {
    /* text block where "text" field is missing — uses empty string */
    const char *body =
        "{\"model\":\"m\","
        "\"content\":[{\"type\":\"text\"}]}";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(g_cb_success);
    ck_assert_uint_eq(g_cb_response->content_count, (size_t)1);
    ck_assert_str_eq(g_cb_response->content_blocks[0].data.text.text, "");
}
END_TEST

START_TEST(test_parse_null_body) {
    /* body == NULL → parse_response returns NULL → failure path */
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = NULL;
    g_mock_http.response_len = 0;

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(g_cb_called);
    ck_assert(!g_cb_success);
    ck_assert_ptr_null(g_cb_response);
    ck_assert_ptr_nonnull(g_cb_error_message);
}
END_TEST

START_TEST(test_parse_zero_body_len) {
    /* body != NULL but body_len == 0 → covers right side of || operator */
    static char body_buf[1] = {0};
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = body_buf;
    g_mock_http.response_len = 0;

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!g_cb_success);
}
END_TEST

START_TEST(test_usage_tokens_not_int) {
    /* usage present but tokens are strings, not ints */
    const char *body =
        "{\"model\":\"m\","
        "\"usage\":{\"input_tokens\":\"x\",\"output_tokens\":\"y\"}}";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(g_cb_success);
    /* tokens default to 0 when not ints */
    ck_assert_int_eq(g_cb_response->usage.input_tokens, 0);
    ck_assert_int_eq(g_cb_response->usage.output_tokens, 0);
}
END_TEST

START_TEST(test_parse_invalid_json) {
    /* Malformed JSON → parse_response returns NULL */
    const char *body = "not-json{{{";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!g_cb_success);
    ck_assert_ptr_null(g_cb_response);
}
END_TEST

START_TEST(test_parse_json_array_not_object) {
    /* Root is array not object → parse_response returns NULL */
    const char *body = "[1,2,3]";
    g_mock_http.type = IK_HTTP_SUCCESS;
    g_mock_http.http_code = 200;
    g_mock_http.response_body = (char *)(uintptr_t)body;
    g_mock_http.response_len = strlen(body);

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!g_cb_success);
}
END_TEST

/* Tests: HTTP error paths */
START_TEST(test_http_error_401) {
    g_mock_http.type = IK_HTTP_CLIENT_ERROR;
    g_mock_http.http_code = 401;
    g_mock_http.error_message = (char *)(uintptr_t)"Unauthorized";

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!g_cb_success);
    ck_assert_int_eq(g_cb_error_category, IK_ERR_CAT_AUTH);
    ck_assert_str_eq(g_cb_error_message, "Unauthorized");
}
END_TEST

START_TEST(test_http_error_403) {
    g_mock_http.type = IK_HTTP_CLIENT_ERROR;
    g_mock_http.http_code = 403;
    g_mock_http.error_message = NULL;

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!g_cb_success);
    ck_assert_int_eq(g_cb_error_category, IK_ERR_CAT_AUTH);
    /* No error_message → talloc_asprintf "HTTP 403" */
    ck_assert_ptr_nonnull(g_cb_error_message);
}
END_TEST

START_TEST(test_http_error_429) {
    g_mock_http.type = IK_HTTP_CLIENT_ERROR;
    g_mock_http.http_code = 429;
    g_mock_http.error_message = NULL;

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!g_cb_success);
    ck_assert_int_eq(g_cb_error_category, IK_ERR_CAT_RATE_LIMIT);
}
END_TEST

START_TEST(test_http_error_500) {
    g_mock_http.type = IK_HTTP_SERVER_ERROR;
    g_mock_http.http_code = 500;
    g_mock_http.error_message = NULL;

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!g_cb_success);
    ck_assert_int_eq(g_cb_error_category, IK_ERR_CAT_SERVER);
}
END_TEST

START_TEST(test_http_error_network) {
    /* http_code == 0 → NETWORK error */
    g_mock_http.type = IK_HTTP_NETWORK_ERROR;
    g_mock_http.http_code = 0;
    g_mock_http.error_message = (char *)(uintptr_t)"Connection refused";

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!g_cb_success);
    ck_assert_int_eq(g_cb_error_category, IK_ERR_CAT_NETWORK);
}
END_TEST

START_TEST(test_http_error_400_unknown) {
    /* 400 → falls through to IK_ERR_CAT_UNKNOWN */
    g_mock_http.type = IK_HTTP_CLIENT_ERROR;
    g_mock_http.http_code = 400;
    g_mock_http.error_message = NULL;

    ik_request_t req = make_req("claude-3");
    ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(!g_cb_success);
    ck_assert_int_eq(g_cb_error_category, IK_ERR_CAT_UNKNOWN);
}
END_TEST

START_TEST(test_add_request_failure) {
    /* ik_http_multi_add_request_ returns ERR → start_request returns ERR */
    g_add_request_fail = true;

    ik_request_t req = make_req("claude-3");
    res_t r = ik_anthropic_start_request(impl_ctx, &req, capture_cb, NULL);

    ck_assert(is_err(&r));
    ck_assert(!g_cb_called);
}
END_TEST

static Suite *anthropic_response_http_suite(void)
{
    Suite *s = suite_create("Anthropic Response HTTP Coverage");

    TCase *tc_parse = tcase_create("Parse Response");
    tcase_set_timeout(tc_parse, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_parse, setup, teardown);
    tcase_add_test(tc_parse, test_success_full_response);
    tcase_add_test(tc_parse, test_success_no_model_field);
    tcase_add_test(tc_parse, test_success_stop_reason_not_string);
    tcase_add_test(tc_parse, test_success_non_text_content_block);
    tcase_add_test(tc_parse, test_success_content_block_no_type);
    tcase_add_test(tc_parse, test_success_text_null_field);
    tcase_add_test(tc_parse, test_parse_null_body);
    tcase_add_test(tc_parse, test_parse_zero_body_len);
    tcase_add_test(tc_parse, test_usage_tokens_not_int);
    tcase_add_test(tc_parse, test_parse_invalid_json);
    tcase_add_test(tc_parse, test_parse_json_array_not_object);
    suite_add_tcase(s, tc_parse);

    TCase *tc_http = tcase_create("HTTP Completion Callback");
    tcase_set_timeout(tc_http, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_http, setup, teardown);
    tcase_add_test(tc_http, test_http_error_401);
    tcase_add_test(tc_http, test_http_error_403);
    tcase_add_test(tc_http, test_http_error_429);
    tcase_add_test(tc_http, test_http_error_500);
    tcase_add_test(tc_http, test_http_error_network);
    tcase_add_test(tc_http, test_http_error_400_unknown);
    tcase_add_test(tc_http, test_add_request_failure);
    suite_add_tcase(s, tc_http);

    return s;
}

#pragma GCC diagnostic pop

int main(void)
{
    Suite *s = anthropic_response_http_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/providers/anthropic/anthropic_response_http_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
