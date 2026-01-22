#include "../../../test_constants.h"
/**
 * @file openai_handlers_test.c
 * @brief Unit tests for OpenAI HTTP completion handlers
 */

#include <check.h>
#include <string.h>
#include <talloc.h>

#include "providers/common/http_multi.h"
#include "providers/openai/error.h"
#include "providers/openai/openai.h"
#include "providers/openai/openai_handlers.h"
#include "providers/openai/response.h"
#include "providers/openai/streaming.h"
#include "providers/provider.h"

/* ================================================================
 * Test Context
 * ================================================================ */

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Mock State for Callbacks
 * ================================================================ */

typedef struct {
    bool called;
    ik_provider_completion_t completion;
    char *error_msg_copy;
} callback_state_t;

static void reset_callback_state(callback_state_t *state)
{
    state->called = false;
    state->completion = (ik_provider_completion_t){0};
    if (state->error_msg_copy) {
        talloc_free(state->error_msg_copy);
        state->error_msg_copy = NULL;
    }
}

static res_t provider_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    callback_state_t *state = (callback_state_t *)ctx;
    state->called = true;
    state->completion = *completion;

    if (completion->error_message) {
        state->error_msg_copy = talloc_strdup(test_ctx, completion->error_message);
    }

    return OK(NULL);
}

/* ================================================================
 * HTTP Completion Handler Tests - Success Cases
 * ================================================================ */

START_TEST(test_http_completion_success_chat_api) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    char *response_body = talloc_strdup(test_ctx, "{\"choices\":[{\"message\":{\"content\":\"Hello\"}}]}");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = response_body,
        .response_len = strlen(response_body)
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(cb_state.completion.success);
    ck_assert_ptr_nonnull(cb_state.completion.response);
    ck_assert_ptr_null(cb_state.completion.error_message);
}
END_TEST

START_TEST(test_http_completion_success_responses_api) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = true;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    char *response_body = talloc_strdup(test_ctx, "{\"status\":\"completed\",\"output\":[]}");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = response_body,
        .response_len = strlen(response_body)
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(cb_state.completion.success);
    ck_assert_ptr_nonnull(cb_state.completion.response);
}

END_TEST
/* ================================================================
 * HTTP Completion Handler Tests - Error Cases
 * ================================================================ */

START_TEST(test_http_completion_error_with_json_body) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    char *error_body = talloc_strdup(test_ctx, "{\"error\":{\"message\":\"Invalid API key\"}}");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_CLIENT_ERROR,
        .http_code = 401,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = error_body,
        .response_len = strlen(error_body)
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_AUTH);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
}

END_TEST

START_TEST(test_http_completion_error_parse_error_fails) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    char *error_body = talloc_strdup(test_ctx, "{\"error\":{\"message\":\"Invalid\"}}");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_CLIENT_ERROR,
        .http_code = 400,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = error_body,
        .response_len = strlen(error_body)
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_INVALID_ARG);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);

}

END_TEST

START_TEST(test_http_completion_error_no_body) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SERVER_ERROR,
        .http_code = 500,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = NULL,
        .response_len = 0
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_UNKNOWN);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
    ck_assert(strstr(cb_state.error_msg_copy, "500") != NULL);
}

END_TEST

START_TEST(test_http_completion_network_error) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    char *error_msg = talloc_strdup(test_ctx, "Could not resolve host");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_NETWORK_ERROR,
        .http_code = 0,
        .curl_code = 6,
        .error_message = error_msg,
        .response_body = NULL,
        .response_len = 0
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_NETWORK);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
    ck_assert(strstr(cb_state.error_msg_copy, "resolve") != NULL);
}

END_TEST

START_TEST(test_http_completion_network_error_no_message) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    ik_http_completion_t http_completion = {
        .type = IK_HTTP_NETWORK_ERROR,
        .http_code = 0,
        .curl_code = 6,
        .error_message = NULL,
        .response_body = NULL,
        .response_len = 0
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_NETWORK);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
}

END_TEST

START_TEST(test_http_completion_parse_response_failure) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    char *response_body = talloc_strdup(test_ctx, "not valid json at all");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = response_body,
        .response_len = strlen(response_body)
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_UNKNOWN);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
    ck_assert(strstr(cb_state.error_msg_copy, "parse") != NULL ||
              strstr(cb_state.error_msg_copy, "Failed") != NULL);
}

END_TEST

START_TEST(test_http_completion_error_parse_error_invalid_json) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    char *error_body = talloc_strdup(test_ctx, "malformed json [[[");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_CLIENT_ERROR,
        .http_code = 400,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = error_body,
        .response_len = strlen(error_body)
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_INVALID_ARG);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
}

END_TEST

START_TEST(test_http_completion_error_with_empty_body) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->cb = provider_completion_cb;
    req_ctx->cb_ctx = &cb_state;

    char *error_body = talloc_strdup(test_ctx, "");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_CLIENT_ERROR,
        .http_code = 403,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = error_body,
        .response_len = 0
    };

    ik_openai_http_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_UNKNOWN);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
    ck_assert(strstr(cb_state.error_msg_copy, "403") != NULL);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_handlers_suite(void)
{
    Suite *s = suite_create("OpenAI Handlers");

    TCase *tc_http_success = tcase_create("HTTP Completion Success");
    tcase_set_timeout(tc_http_success, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_http_success, setup, teardown);
    tcase_add_test(tc_http_success, test_http_completion_success_chat_api);
    tcase_add_test(tc_http_success, test_http_completion_success_responses_api);
    suite_add_tcase(s, tc_http_success);

    TCase *tc_http_error = tcase_create("HTTP Completion Errors");
    tcase_set_timeout(tc_http_error, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_http_error, setup, teardown);
    tcase_add_test(tc_http_error, test_http_completion_error_with_json_body);
    tcase_add_test(tc_http_error, test_http_completion_error_parse_error_fails);
    tcase_add_test(tc_http_error, test_http_completion_error_no_body);
    tcase_add_test(tc_http_error, test_http_completion_network_error);
    tcase_add_test(tc_http_error, test_http_completion_network_error_no_message);
    tcase_add_test(tc_http_error, test_http_completion_parse_response_failure);
    tcase_add_test(tc_http_error, test_http_completion_error_parse_error_invalid_json);
    tcase_add_test(tc_http_error, test_http_completion_error_with_empty_body);
    suite_add_tcase(s, tc_http_error);

    return s;
}

int main(void)
{
    Suite *s = openai_handlers_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/openai/openai_handlers_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
