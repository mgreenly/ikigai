/**
 * @file repl_http_completion_callback_test.c
 * @brief Unit tests for REPL HTTP completion callback
 *
 * Tests the ik_repl_http_completion_callback function which handles
 * HTTP request completion for both success and error cases.
 */

#include "repl.h"
#include "../../../src/agent.h"
#include "repl_callbacks.h"
#include "shared.h"
#include "scrollback.h"
#include "config.h"
#include "openai/client_multi.h"
#include "tool.h"
#include "debug_pipe.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>
#include <unistd.h>

static void *ctx;
static ik_repl_ctx_t *repl;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);

    /* Create minimal REPL context for testing callback */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;
    repl->current->scrollback = ik_scrollback_create(repl, 80);
    repl->streaming_line_buffer = NULL;
    repl->http_error_message = NULL;
    repl->response_model = NULL;
    repl->response_finish_reason = NULL;
    repl->response_completion_tokens = 0;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/* Test: Flush remaining buffered line content when completion occurs */
START_TEST(test_completion_flushes_streaming_buffer) {
    /* Simulate partial streaming content in buffer */
    repl->streaming_line_buffer = talloc_strdup(repl, "Partial line content");

    /* Create successful completion */
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify buffer was flushed to scrollback (content + blank line) and cleared */
    ck_assert_ptr_null(repl->streaming_line_buffer);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(repl->current->scrollback), 2);
}
END_TEST
/* Test: Completion clears previous error message */
START_TEST(test_completion_clears_previous_error)
{
    /* Set up previous error */
    repl->http_error_message = talloc_strdup(repl, "Previous error");

    /* Create successful completion */
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify error was cleared */
    ck_assert_ptr_null(repl->http_error_message);
}

END_TEST
/* Test: Completion stores error message on failure */
START_TEST(test_completion_stores_error_on_failure)
{
    /* Create failed completion */
    char *error_msg = talloc_strdup(ctx, "HTTP 500 server error");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SERVER_ERROR,
        .http_code = 500,
        .curl_code = CURLE_OK,
        .error_message = error_msg,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify error message was stored */
    ck_assert_ptr_nonnull(repl->http_error_message);
    ck_assert_str_eq(repl->http_error_message, "HTTP 500 server error");
}

END_TEST
/* Test: Completion stores response metadata on success */
START_TEST(test_completion_stores_metadata_on_success)
{
    /* Create successful completion with metadata */
    char *model = talloc_strdup(ctx, "gpt-4-turbo");
    char *finish_reason = talloc_strdup(ctx, "stop");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = model,
        .finish_reason = finish_reason,
        .completion_tokens = 42
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify metadata was stored */
    ck_assert_ptr_nonnull(repl->response_model);
    ck_assert_str_eq(repl->response_model, "gpt-4-turbo");
    ck_assert_ptr_nonnull(repl->response_finish_reason);
    ck_assert_str_eq(repl->response_finish_reason, "stop");
    ck_assert_int_eq(repl->response_completion_tokens, 42);
}

END_TEST
/* Test: Completion clears previous metadata before storing new */
START_TEST(test_completion_clears_previous_metadata)
{
    /* Set up previous metadata */
    repl->response_model = talloc_strdup(repl, "old-model");
    repl->response_finish_reason = talloc_strdup(repl, "old-reason");
    repl->response_completion_tokens = 99;

    /* Create successful completion with new metadata */
    char *model = talloc_strdup(ctx, "new-model");
    char *finish_reason = talloc_strdup(ctx, "new-reason");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = model,
        .finish_reason = finish_reason,
        .completion_tokens = 50
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify old metadata was replaced */
    ck_assert_str_eq(repl->response_model, "new-model");
    ck_assert_str_eq(repl->response_finish_reason, "new-reason");
    ck_assert_int_eq(repl->response_completion_tokens, 50);
}

END_TEST
/* Test: Completion with NULL metadata doesn't store anything */
START_TEST(test_completion_null_metadata)
{
    /* Create successful completion with NULL metadata */
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify no metadata was stored */
    ck_assert_ptr_null(repl->response_model);
    ck_assert_ptr_null(repl->response_finish_reason);
    ck_assert_int_eq(repl->response_completion_tokens, 0);
}

END_TEST
/* Test: Completion with network error stores error message */
START_TEST(test_completion_network_error)
{
    /* Create network error completion */
    char *error_msg = talloc_strdup(ctx, "Connection error: Failed to connect");
    ik_http_completion_t completion = {
        .type = IK_HTTP_NETWORK_ERROR,
        .http_code = 0,
        .curl_code = CURLE_COULDNT_CONNECT,
        .error_message = error_msg,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify error message was stored */
    ck_assert_ptr_nonnull(repl->http_error_message);
    ck_assert_str_eq(repl->http_error_message, "Connection error: Failed to connect");
}

END_TEST
/* Test: Completion with client error (4xx) stores error */
START_TEST(test_completion_client_error)
{
    /* Create client error completion */
    char *error_msg = talloc_strdup(ctx, "HTTP 401 error");
    ik_http_completion_t completion = {
        .type = IK_HTTP_CLIENT_ERROR,
        .http_code = 401,
        .curl_code = CURLE_OK,
        .error_message = error_msg,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify error message was stored */
    ck_assert_ptr_nonnull(repl->http_error_message);
    ck_assert_str_eq(repl->http_error_message, "HTTP 401 error");
}

END_TEST
/* Test: Both streaming buffer and error handling */
START_TEST(test_completion_flushes_buffer_and_stores_error)
{
    /* Set up partial streaming content */
    repl->streaming_line_buffer = talloc_strdup(repl, "Incomplete response");

    /* Create failed completion */
    char *error_msg = talloc_strdup(ctx, "Request timeout");
    ik_http_completion_t completion = {
        .type = IK_HTTP_NETWORK_ERROR,
        .http_code = 0,
        .curl_code = CURLE_OPERATION_TIMEDOUT,
        .error_message = error_msg,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify buffer was flushed */
    ck_assert_ptr_null(repl->streaming_line_buffer);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(repl->current->scrollback), 1);

    /* Verify error was stored */
    ck_assert_ptr_nonnull(repl->http_error_message);
    ck_assert_str_eq(repl->http_error_message, "Request timeout");
}

END_TEST
/* Test: Error completion with NULL error_message doesn't store error */
START_TEST(test_completion_error_null_message)
{
    /* Create error completion with NULL error message */
    ik_http_completion_t completion = {
        .type = IK_HTTP_SERVER_ERROR,
        .http_code = 500,
        .curl_code = CURLE_OK,
        .error_message = NULL,  /* NULL error message on error */
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify no error message was stored (NULL && branch) */
    ck_assert_ptr_null(repl->http_error_message);
}

END_TEST
/* Test: Completion stores tool_call in pending_tool_call */
START_TEST(test_completion_stores_tool_call)
{
    /* Create tool_call */
    ik_tool_call_t *tc = ik_tool_call_create(ctx, "call_test123", "glob", "{\"pattern\": \"*.c\"}");

    /* Create successful completion with tool_call */
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = NULL,
        .finish_reason = talloc_strdup(ctx, "tool_calls"),
        .completion_tokens = 50,
        .tool_call = tc
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify tool_call was stored */
    ck_assert_ptr_nonnull(repl->pending_tool_call);
    ck_assert_str_eq(repl->pending_tool_call->id, "call_test123");
    ck_assert_str_eq(repl->pending_tool_call->name, "glob");
    ck_assert_str_eq(repl->pending_tool_call->arguments, "{\"pattern\": \"*.c\"}");
}

END_TEST
/* Test: Completion clears previous pending_tool_call before storing new one */
START_TEST(test_completion_clears_previous_tool_call)
{
    /* Set up previous pending_tool_call */
    repl->pending_tool_call = ik_tool_call_create(repl, "old_call", "old_tool", "{}");

    /* Create new tool_call */
    ik_tool_call_t *tc = ik_tool_call_create(ctx, "new_call", "new_tool", "{\"key\": \"value\"}");

    /* Create successful completion with tool_call */
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = NULL,
        .finish_reason = talloc_strdup(ctx, "tool_calls"),
        .completion_tokens = 25,
        .tool_call = tc
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify new tool_call replaced old one */
    ck_assert_ptr_nonnull(repl->pending_tool_call);
    ck_assert_str_eq(repl->pending_tool_call->id, "new_call");
    ck_assert_str_eq(repl->pending_tool_call->name, "new_tool");
}

END_TEST
/* Test: Completion with NULL tool_call clears pending_tool_call */
START_TEST(test_completion_null_tool_call_clears_pending)
{
    /* Set up previous pending_tool_call */
    repl->pending_tool_call = ik_tool_call_create(repl, "old_call", "old_tool", "{}");

    /* Create successful completion without tool_call */
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = NULL,
        .finish_reason = talloc_strdup(ctx, "stop"),
        .completion_tokens = 10,
        .tool_call = NULL
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Verify pending_tool_call was cleared */
    ck_assert_ptr_null(repl->pending_tool_call);
}

END_TEST

/*
 * Test suite
 */

static Suite *repl_http_completion_callback_suite(void)
{
    Suite *s = suite_create("repl_http_completion_callback");

    TCase *tc_core = tcase_create("callback_behavior");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_completion_flushes_streaming_buffer);
    tcase_add_test(tc_core, test_completion_clears_previous_error);
    tcase_add_test(tc_core, test_completion_stores_error_on_failure);
    tcase_add_test(tc_core, test_completion_stores_metadata_on_success);
    tcase_add_test(tc_core, test_completion_clears_previous_metadata);
    tcase_add_test(tc_core, test_completion_null_metadata);
    tcase_add_test(tc_core, test_completion_network_error);
    tcase_add_test(tc_core, test_completion_client_error);
    tcase_add_test(tc_core, test_completion_flushes_buffer_and_stores_error);
    tcase_add_test(tc_core, test_completion_error_null_message);
    tcase_add_test(tc_core, test_completion_stores_tool_call);
    tcase_add_test(tc_core, test_completion_clears_previous_tool_call);
    tcase_add_test(tc_core, test_completion_null_tool_call_clears_pending);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_http_completion_callback_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
