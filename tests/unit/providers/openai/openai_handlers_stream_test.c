/**
 * @file openai_handlers_stream_test.c
 * @brief Unit tests for OpenAI HTTP streaming handlers
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

/* Test Context */

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* Mock State for Callbacks */

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

/* Stream Write Callback Tests */

/* Dummy stream callback for parser context */
static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

START_TEST(test_stream_write_callback_single_line) {
    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->sse_buffer = NULL;
    req_ctx->sse_buffer_len = 0;
    req_ctx->parser_ctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);

    const char *data = "data: {\"id\":\"123\"}\n";
    size_t len = strlen(data);

    size_t result = ik_openai_stream_write_callback(data, len, req_ctx);

    ck_assert_uint_eq(result, len);
    ck_assert_uint_eq(req_ctx->sse_buffer_len, 0);
    ck_assert_ptr_null(req_ctx->sse_buffer);
}
END_TEST

START_TEST(test_stream_write_callback_partial_line) {
    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->sse_buffer = NULL;
    req_ctx->sse_buffer_len = 0;
    req_ctx->parser_ctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);

    const char *data = "data: {\"id\":\"123\"";
    size_t len = strlen(data);

    size_t result = ik_openai_stream_write_callback(data, len, req_ctx);

    ck_assert_uint_eq(result, len);
    ck_assert_uint_eq(req_ctx->sse_buffer_len, len);
    ck_assert_ptr_nonnull(req_ctx->sse_buffer);
    ck_assert_str_eq(req_ctx->sse_buffer, data);
}

END_TEST

START_TEST(test_stream_write_callback_continuation) {
    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->sse_buffer = talloc_strdup(req_ctx, "data: {\"id\":\"");
    req_ctx->sse_buffer_len = strlen(req_ctx->sse_buffer);
    req_ctx->parser_ctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);

    const char *data = "123\"}\n";
    size_t len = strlen(data);

    size_t result = ik_openai_stream_write_callback(data, len, req_ctx);

    ck_assert_uint_eq(result, len);
    ck_assert_uint_eq(req_ctx->sse_buffer_len, 0);
    ck_assert_ptr_null(req_ctx->sse_buffer);
}

END_TEST

START_TEST(test_stream_write_callback_multiple_lines) {
    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->sse_buffer = NULL;
    req_ctx->sse_buffer_len = 0;
    req_ctx->parser_ctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);

    const char *data = "data: {\"id\":\"1\"}\ndata: {\"id\":\"2\"}\n";
    size_t len = strlen(data);

    size_t result = ik_openai_stream_write_callback(data, len, req_ctx);

    ck_assert_uint_eq(result, len);
    ck_assert_uint_eq(req_ctx->sse_buffer_len, 0);
}

END_TEST

START_TEST(test_stream_write_callback_non_data_line) {
    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->sse_buffer = NULL;
    req_ctx->sse_buffer_len = 0;
    req_ctx->parser_ctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);

    const char *data = "event: ping\n";
    size_t len = strlen(data);

    size_t result = ik_openai_stream_write_callback(data, len, req_ctx);

    ck_assert_uint_eq(result, len);
    ck_assert_uint_eq(req_ctx->sse_buffer_len, 0);
}

END_TEST

START_TEST(test_stream_write_callback_responses_api) {
    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = true;
    req_ctx->parser_ctx = ik_openai_responses_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);

    const char *data = "event: response.done\ndata: {}\n\n";
    size_t len = strlen(data);

    size_t result = ik_openai_stream_write_callback(data, len, req_ctx);

    ck_assert_uint_eq(result, len);
}

END_TEST
/* Stream Completion Handler Tests */

START_TEST(test_stream_completion_success) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->completion_cb = provider_completion_cb;
    req_ctx->completion_ctx = &cb_state;

    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = NULL,
        .response_len = 0
    };

    ik_openai_stream_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(cb_state.completion.success);
    ck_assert_ptr_null(cb_state.completion.response);
    ck_assert_ptr_null(cb_state.completion.error_message);
}

END_TEST

START_TEST(test_stream_completion_error_with_json) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->completion_cb = provider_completion_cb;
    req_ctx->completion_ctx = &cb_state;

    char *error_body = talloc_strdup(test_ctx, "{\"error\":{\"message\":\"Rate limit\"}}");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_CLIENT_ERROR,
        .http_code = 429,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = error_body,
        .response_len = strlen(error_body)
    };

    ik_openai_stream_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_RATE_LIMIT);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
}

END_TEST

START_TEST(test_stream_completion_error_parse_fails) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->completion_cb = provider_completion_cb;
    req_ctx->completion_ctx = &cb_state;

    char *error_body = talloc_strdup(test_ctx, "{\"error\":{\"message\":\"Error\"}}");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SERVER_ERROR,
        .http_code = 503,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = error_body,
        .response_len = strlen(error_body)
    };

    ik_openai_stream_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);

}

END_TEST

START_TEST(test_stream_completion_error_no_body) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->completion_cb = provider_completion_cb;
    req_ctx->completion_ctx = &cb_state;

    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SERVER_ERROR,
        .http_code = 500,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = NULL,
        .response_len = 0
    };

    ik_openai_stream_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_UNKNOWN);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
}

END_TEST

START_TEST(test_stream_completion_network_error) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->completion_cb = provider_completion_cb;
    req_ctx->completion_ctx = &cb_state;

    ik_http_completion_t http_completion = {
        .type = IK_HTTP_NETWORK_ERROR,
        .http_code = 0,
        .curl_code = 7,
        .error_message = talloc_strdup(test_ctx, "Failed to connect"),
        .response_body = NULL,
        .response_len = 0
    };

    ik_openai_stream_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_NETWORK);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
    ck_assert(strstr(cb_state.error_msg_copy, "connect") != NULL);
}

END_TEST

START_TEST(test_stream_completion_network_error_no_message) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->completion_cb = provider_completion_cb;
    req_ctx->completion_ctx = &cb_state;

    ik_http_completion_t http_completion = {
        .type = IK_HTTP_NETWORK_ERROR,
        .http_code = 0,
        .curl_code = 7,
        .error_message = NULL,
        .response_body = NULL,
        .response_len = 0
    };

    ik_openai_stream_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_NETWORK);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
}

END_TEST

START_TEST(test_stream_completion_error_parse_error_invalid_json) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->completion_cb = provider_completion_cb;
    req_ctx->completion_ctx = &cb_state;

    char *error_body = talloc_strdup(test_ctx, "invalid {{{");
    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SERVER_ERROR,
        .http_code = 500,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = error_body,
        .response_len = strlen(error_body)
    };

    ik_openai_stream_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(!cb_state.completion.success);
    ck_assert_int_eq(cb_state.completion.error_category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(cb_state.error_msg_copy);
}

END_TEST

START_TEST(test_stream_completion_success_with_chat_parser) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = false;
    req_ctx->completion_cb = provider_completion_cb;
    req_ctx->completion_ctx = &cb_state;
    req_ctx->parser_ctx = ik_openai_chat_stream_ctx_create(req_ctx, dummy_stream_cb, NULL);

    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = NULL,
        .response_len = 0
    };

    ik_openai_stream_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(cb_state.completion.success);
    ck_assert_ptr_null(cb_state.completion.error_message);
}

END_TEST

START_TEST(test_stream_completion_success_with_responses_parser) {
    callback_state_t cb_state = {0};
    reset_callback_state(&cb_state);

    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(test_ctx, ik_openai_stream_request_ctx_t);
    req_ctx->use_responses_api = true;
    req_ctx->completion_cb = provider_completion_cb;
    req_ctx->completion_ctx = &cb_state;
    req_ctx->parser_ctx = ik_openai_responses_stream_ctx_create(req_ctx, dummy_stream_cb, NULL);

    ik_http_completion_t http_completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = 0,
        .error_message = NULL,
        .response_body = NULL,
        .response_len = 0
    };

    ik_openai_stream_completion_handler(&http_completion, req_ctx);

    ck_assert(cb_state.called);
    ck_assert(cb_state.completion.success);
    ck_assert_ptr_null(cb_state.completion.error_message);
}

END_TEST

/* Test Suite Setup */

static Suite *openai_handlers_stream_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Handlers");

    TCase *tc_stream_write = tcase_create("Stream Write Callback");
    tcase_set_timeout(tc_stream_write, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_stream_write, setup, teardown);
    tcase_add_test(tc_stream_write, test_stream_write_callback_single_line);
    tcase_add_test(tc_stream_write, test_stream_write_callback_partial_line);
    tcase_add_test(tc_stream_write, test_stream_write_callback_continuation);
    tcase_add_test(tc_stream_write, test_stream_write_callback_multiple_lines);
    tcase_add_test(tc_stream_write, test_stream_write_callback_non_data_line);
    tcase_add_test(tc_stream_write, test_stream_write_callback_responses_api);
    suite_add_tcase(s, tc_stream_write);

    TCase *tc_stream_completion = tcase_create("Stream Completion");
    tcase_set_timeout(tc_stream_completion, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_stream_completion, setup, teardown);
    tcase_add_test(tc_stream_completion, test_stream_completion_success);
    tcase_add_test(tc_stream_completion, test_stream_completion_error_with_json);
    tcase_add_test(tc_stream_completion, test_stream_completion_error_parse_fails);
    tcase_add_test(tc_stream_completion, test_stream_completion_error_no_body);
    tcase_add_test(tc_stream_completion, test_stream_completion_network_error);
    tcase_add_test(tc_stream_completion, test_stream_completion_network_error_no_message);
    tcase_add_test(tc_stream_completion, test_stream_completion_error_parse_error_invalid_json);
    tcase_add_test(tc_stream_completion, test_stream_completion_success_with_chat_parser);
    tcase_add_test(tc_stream_completion, test_stream_completion_success_with_responses_parser);
    suite_add_tcase(s, tc_stream_completion);

    return s;
}

int main(void)
{
    Suite *s = openai_handlers_stream_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
