/**
 * @file repl_debug_response_test.c
 * @brief Unit tests for debug output in HTTP completion callback
 *
 * Tests the debug response metadata output when completion callback
 * fires with different response types and metadata.
 */

#include "repl.h"
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
    repl->scrollback = ik_scrollback_create(repl, 80);
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

/* Test: Debug output for successful response with metadata */
START_TEST(test_debug_output_response_success) {
    /* Create debug pipe */
    int pipefd[2];
    int ret = pipe(pipefd);
    ck_assert_int_eq(ret, 0);

    ik_debug_pipe_t *debug_pipe = talloc_zero(ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);
    debug_pipe->write_end = fdopen(pipefd[1], "w");
    ck_assert_ptr_nonnull(debug_pipe->write_end);
    repl->shared->openai_debug_pipe = debug_pipe;

    /* Create successful completion with metadata */
    char *model = talloc_strdup(ctx, "gpt-4o");
    char *finish_reason = talloc_strdup(ctx, "stop");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = model,
        .finish_reason = finish_reason,
        .completion_tokens = 42,
        .tool_call = NULL
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Flush and read debug output */
    fflush(debug_pipe->write_end);
    char buffer[256];
    ssize_t n = read(pipefd[0], buffer, sizeof(buffer) - 1);
    ck_assert_int_gt((int)n, 0);
    buffer[n] = '\0';

    /* Verify debug output format */
    ck_assert_ptr_nonnull(strstr(buffer, "<< RESPONSE: type=success"));
    ck_assert_ptr_nonnull(strstr(buffer, "model=gpt-4o"));
    ck_assert_ptr_nonnull(strstr(buffer, "finish=stop"));
    ck_assert_ptr_nonnull(strstr(buffer, "tokens=42"));

    /* Cleanup */
    fclose(debug_pipe->write_end);
    close(pipefd[0]);
}

END_TEST
/* Test: Debug output for error response */
START_TEST(test_debug_output_response_error)
{
    /* Create debug pipe */
    int pipefd[2];
    int ret = pipe(pipefd);
    ck_assert_int_eq(ret, 0);

    ik_debug_pipe_t *debug_pipe = talloc_zero(ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);
    debug_pipe->write_end = fdopen(pipefd[1], "w");
    ck_assert_ptr_nonnull(debug_pipe->write_end);
    repl->shared->openai_debug_pipe = debug_pipe;

    /* Create error completion */
    char *error_msg = talloc_strdup(ctx, "HTTP 500 server error");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SERVER_ERROR,
        .http_code = 500,
        .curl_code = CURLE_OK,
        .error_message = error_msg,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0,
        .tool_call = NULL
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Flush and read debug output */
    fflush(debug_pipe->write_end);
    char buffer[256];
    ssize_t n = read(pipefd[0], buffer, sizeof(buffer) - 1);
    ck_assert_int_gt((int)n, 0);
    buffer[n] = '\0';

    /* Verify debug output format - only type for errors */
    ck_assert_ptr_nonnull(strstr(buffer, "<< RESPONSE: type=error"));

    /* Cleanup */
    fclose(debug_pipe->write_end);
    close(pipefd[0]);
}

END_TEST
/* Test: Debug output with tool_call information */
START_TEST(test_debug_output_response_with_tool_call)
{
    /* Create debug pipe */
    int pipefd[2];
    int ret = pipe(pipefd);
    ck_assert_int_eq(ret, 0);

    ik_debug_pipe_t *debug_pipe = talloc_zero(ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);
    debug_pipe->write_end = fdopen(pipefd[1], "w");
    ck_assert_ptr_nonnull(debug_pipe->write_end);
    repl->shared->openai_debug_pipe = debug_pipe;

    /* Create tool_call */
    ik_tool_call_t *tc = ik_tool_call_create(ctx, "call_123", "glob", "{\"pattern\":\"*.c\"}");

    /* Create successful completion with tool_call */
    char *model = talloc_strdup(ctx, "gpt-4o");
    char *finish_reason = talloc_strdup(ctx, "tool_calls");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = model,
        .finish_reason = finish_reason,
        .completion_tokens = 50,
        .tool_call = tc
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Flush and read debug output */
    fflush(debug_pipe->write_end);
    char buffer[512];
    ssize_t n = read(pipefd[0], buffer, sizeof(buffer) - 1);
    ck_assert_int_gt((int)n, 0);
    buffer[n] = '\0';

    /* Verify debug output includes tool_call info */
    ck_assert_ptr_nonnull(strstr(buffer, "<< RESPONSE: type=success"));
    ck_assert_ptr_nonnull(strstr(buffer, "model=gpt-4o"));
    ck_assert_ptr_nonnull(strstr(buffer, "finish=tool_calls"));
    ck_assert_ptr_nonnull(strstr(buffer, "tokens=50"));
    ck_assert_ptr_nonnull(strstr(buffer, "tool_call=glob({\"pattern\":\"*.c\"})"));

    /* Cleanup */
    fclose(debug_pipe->write_end);
    close(pipefd[0]);
}

END_TEST
/* Test: Debug output with NULL model and finish_reason */
START_TEST(test_debug_output_null_metadata)
{
    /* Create debug pipe */
    int pipefd[2];
    int ret = pipe(pipefd);
    ck_assert_int_eq(ret, 0);

    ik_debug_pipe_t *debug_pipe = talloc_zero(ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);
    debug_pipe->write_end = fdopen(pipefd[1], "w");
    ck_assert_ptr_nonnull(debug_pipe->write_end);
    repl->shared->openai_debug_pipe = debug_pipe;

    /* Create successful completion with NULL metadata */
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0,
        .tool_call = NULL
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));

    /* Flush and read debug output */
    fflush(debug_pipe->write_end);
    char buffer[256];
    ssize_t n = read(pipefd[0], buffer, sizeof(buffer) - 1);
    ck_assert_int_gt((int)n, 0);
    buffer[n] = '\0';

    /* Verify debug output shows (null) for missing metadata */
    ck_assert_ptr_nonnull(strstr(buffer, "<< RESPONSE: type=success"));
    ck_assert_ptr_nonnull(strstr(buffer, "model=(null)"));
    ck_assert_ptr_nonnull(strstr(buffer, "finish=(null)"));
    ck_assert_ptr_nonnull(strstr(buffer, "tokens=0"));

    /* Cleanup */
    fclose(debug_pipe->write_end);
    close(pipefd[0]);
}

END_TEST
/* Test: No debug output when openai_debug_pipe is NULL */
START_TEST(test_debug_output_no_pipe)
{
    /* Set openai_debug_pipe to NULL */
    repl->shared->openai_debug_pipe = NULL;

    /* Create successful completion */
    char *model = talloc_strdup(ctx, "gpt-4o");
    char *finish_reason = talloc_strdup(ctx, "stop");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = model,
        .finish_reason = finish_reason,
        .completion_tokens = 42,
        .tool_call = NULL
    };

    /* Call callback - should not crash */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));
}

END_TEST
/* Test: No debug output when openai_debug_pipe->write_end is NULL */
START_TEST(test_debug_output_null_write_end)
{
    /* Create debug pipe with NULL write_end */
    ik_debug_pipe_t *debug_pipe = talloc_zero(ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);
    debug_pipe->write_end = NULL;
    repl->shared->openai_debug_pipe = debug_pipe;

    /* Create successful completion */
    char *model = talloc_strdup(ctx, "gpt-4o");
    char *finish_reason = talloc_strdup(ctx, "stop");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = model,
        .finish_reason = finish_reason,
        .completion_tokens = 42,
        .tool_call = NULL
    };

    /* Call callback - should not crash */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
    ck_assert(is_ok(&result));
}

END_TEST

/*
 * Test suite
 */

static Suite *repl_debug_response_suite(void)
{
    Suite *s = suite_create("repl_debug_response");

    TCase *tc_debug = tcase_create("debug_output");
    tcase_add_checked_fixture(tc_debug, setup, teardown);
    tcase_add_test(tc_debug, test_debug_output_response_success);
    tcase_add_test(tc_debug, test_debug_output_response_error);
    tcase_add_test(tc_debug, test_debug_output_response_with_tool_call);
    tcase_add_test(tc_debug, test_debug_output_null_metadata);
    tcase_add_test(tc_debug, test_debug_output_no_pipe);
    tcase_add_test(tc_debug, test_debug_output_null_write_end);
    suite_add_tcase(s, tc_debug);

    return s;
}

int main(void)
{
    Suite *s = repl_debug_response_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
